#include <kernel/app.h>
#include <kernel/ini.h>
#include <kernel/loader.h>
#include <kernel/cpu.h>
#include <kernel/tss.h>
#include <kernel/mem.h>
#include <arch/acpi.h>
#include <fs/vfs.h>
#include <drivers/monitor.h>

AppSlot app_table[MAX_APPS];
uint8   core_to_app[16];   // MAX_CPUS, 0xFF = free

void app_init(void) {
    for (int i = 0; i < MAX_APPS; i++)
        app_table[i].active = 0;
    for (int i = 0; i < 16; i++)
        core_to_app[i] = 0xFF;
}

// INI parse callback: populate AppManifest
static void manifest_handler(const char *section, const char *key,
                             const char *value, void *user) {
    AppManifest *m = (AppManifest *)user;
    (void)section;

    if (key[0] == 'n' && key[1] == 'a') {
        // name
        for (int i = 0; i < APP_NAME_LEN - 1 && value[i]; i++)
            m->name[i] = value[i];
    } else if (key[0] == 'b') {
        // binary
        for (int i = 0; i < APP_PATH_LEN - 1 && value[i]; i++)
            m->binary[i] = value[i];
    } else if (key[0] == 'c') {
        // cores=1,2,3
        m->core_count = 0;
        const char *p = value;
        while (*p && m->core_count < MAX_APP_CORES) {
            uint32 n = 0;
            while (*p >= '0' && *p <= '9') {
                n = n * 10 + (*p - '0');
                p++;
            }
            if (n > 0 && n < 16)
                m->cores[m->core_count++] = n;
            if (*p == ',') p++;
        }
    }
}

static int find_free_slot(void) {
    for (int i = 0; i < MAX_APPS; i++)
        if (!app_table[i].active) return i;
    return -1;
}

int app_launch(const char *manifest_path) {
    // Read manifest from VFS
    uint32 msize = vfs_file_size(manifest_path);
    if (msize == 0) {
        kprint("APP: manifest not found: ");
        kprint((char *)manifest_path);
        putc('\n');
        return -1;
    }

    char mbuf[512];
    if (msize > sizeof(mbuf) - 1) msize = sizeof(mbuf) - 1;
    if (vfs_read_file(manifest_path, mbuf, msize) < 0) {
        kprint("APP: failed to read manifest\n");
        return -1;
    }
    mbuf[msize] = '\0';

    // Parse manifest
    AppManifest manifest;
    memset(&manifest, 0, sizeof(manifest));
    ini_parse(mbuf, msize, manifest_handler, &manifest);

    if (manifest.binary[0] == '\0' || manifest.core_count == 0) {
        kprint("APP: invalid manifest (need binary and cores)\n");
        return -1;
    }

    // Find free slot
    int slot = find_free_slot();
    if (slot < 0) {
        kprint("APP: no free slots\n");
        return -1;
    }

    AppSlot *app = &app_table[slot];

    // Validate cores: must be running APs, not in use
    for (uint32 i = 0; i < manifest.core_count; i++) {
        uint32 c = manifest.cores[i];
        if (c == 0 || c >= cpu_count) {
            kprint("APP: invalid core ");
            kprint_dec(c);
            putc('\n');
            return -1;
        }
        if (!percpu[c].running) {
            kprint("APP: core ");
            kprint_dec(c);
            kprint(" not running\n");
            return -1;
        }
        if (core_to_app[c] != 0xFF) {
            kprint("APP: core ");
            kprint_dec(c);
            kprint(" already in use\n");
            return -1;
        }
    }

    // Check NIC conflicts
    memset(app->nic_locked, 0, sizeof(app->nic_locked));
    for (uint32 i = 0; i < manifest.core_count; i++) {
        uint32 nic = thread_meta[manifest.cores[i]].nic_index;
        if (nic == NIC_NONE) continue;
        // Check if locked by another app
        for (int s = 0; s < MAX_APPS; s++) {
            if (s == slot || !app_table[s].active) continue;
            if (app_table[s].nic_locked[nic]) {
                kprint("APP: NIC ");
                kprint_dec(nic);
                kprint(" already locked\n");
                return -1;
            }
        }
        app->nic_locked[nic] = 1;
    }

    // Load binary
    uint64 load_addr = APP_LOAD_BASE + (uint64)slot * APP_REGION_SIZE;
    uint32 file_size = vfs_file_size(manifest.binary);
    if (file_size == 0) {
        kprint("APP: binary not found: ");
        kprint((char *)manifest.binary);
        putc('\n');
        return -1;
    }

    if (vfs_read_file(manifest.binary, (void *)load_addr, file_size) < 0) {
        kprint("APP: failed to read binary\n");
        return -1;
    }

    // Fill slot (zero first to avoid stale data from previous use or uninitialized BSS)
    memset(app, 0, sizeof(AppSlot));
    for (int i = 0; i < APP_NAME_LEN - 1 && manifest.name[i]; i++)
        app->name[i] = manifest.name[i];
    app->load_addr = load_addr;
    app->file_size = file_size;
    app->core_count = manifest.core_count;
    app->cores_done = 0;
    for (uint32 i = 0; i < manifest.core_count; i++)
        app->cores[i] = manifest.cores[i];

    // Mark cores in use
    for (uint32 i = 0; i < manifest.core_count; i++)
        core_to_app[manifest.cores[i]] = (uint8)slot;

    kprint("APP: ");
    kprint(app->name);
    kprint(" loaded at ");
    kprint_long2hex(load_addr, "");
    kprint(" cores:");
    for (uint32 i = 0; i < manifest.core_count; i++) {
        putc(' ');
        kprint_dec(manifest.cores[i]);
    }
    putc('\n');

    // Allocate user stacks and set entry addresses
    for (uint32 i = 0; i < manifest.core_count; i++) {
        uint32 c = manifest.cores[i];
        ap_user_stacks[c] = alloc_pages(USER_STACK_SIZE / 4096);
        ap_entry_addrs[c] = load_addr;
    }

    // Setup ring 3 on each assigned core (sequential)
    for (uint32 i = 0; i < manifest.core_count; i++) {
        ap_dispatch(manifest.cores[i], ap_setup_ring3, manifest.cores[i]);
    }

    // Dispatch ring 3 entry (blocking for now, each core completes before next)
    for (uint32 i = 0; i < manifest.core_count; i++) {
        uint32 c = manifest.cores[i];
        kprint("APP: dispatching to CPU ");
        kprint_dec(c);
        kprint("...\n");
        ap_dispatch(c, ap_run_ring3, c);
        kprint("APP: CPU ");
        kprint_dec(c);
        kprint(" done\n");
    }

    // All cores done, clean up
    for (uint32 i = 0; i < manifest.core_count; i++)
        core_to_app[manifest.cores[i]] = 0xFF;
    memset(app->nic_locked, 0, sizeof(app->nic_locked));
    app->active = 0;

    kprint("APP: ");
    kprint(app->name);
    kprint(" finished\n");
    return 0;
}

int app_check_completion(void) {
    // For future non-blocking dispatch
    return 0;
}

void app_list(void) {
    int found = 0;
    for (int i = 0; i < MAX_APPS; i++) {
        if (!app_table[i].active) continue;
        found = 1;
        kprint("  slot ");
        kprint_dec(i);
        kprint(": ");
        kprint(app_table[i].name);
        kprint(" cores:");
        for (uint32 j = 0; j < app_table[i].core_count; j++) {
            putc(' ');
            kprint_dec(app_table[i].cores[j]);
        }
        kprint(" (");
        kprint_dec(app_table[i].cores_done);
        putc('/');
        kprint_dec(app_table[i].core_count);
        kprint(" done)\n");
    }
    if (!found) kprint("  no running apps\n");
}
