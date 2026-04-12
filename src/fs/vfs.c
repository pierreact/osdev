#include <fs/vfs.h>
#include <fs/iso9660.h>
#include <fs/fat32.h>
#include <kernel/mem.h>
#include <drivers/monitor.h>

static VFSMount mounts[VFS_MAX_MOUNTS];
static uint32 mount_count = 0;

static uint32 str_len(const char *s) {
    uint32 n = 0;
    while (s[n]) n++;
    return n;
}

static int str_starts_with(const char *str, const char *prefix) {
    while (*prefix) {
        if (*str != *prefix) return 0;
        str++; prefix++;
    }
    return 1;
}

static void str_copy(char *dst, const char *src, uint32 max) {
    uint32 i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

void vfs_init(void) {
    mount_count = 0;
    for (uint32 i = 0; i < VFS_MAX_MOUNTS; i++)
        mounts[i].active = 0;
}

int vfs_mount(const char *mountpoint, uint8 fs_type, uint32 device) {
    if (mount_count >= VFS_MAX_MOUNTS) return -1;

    // Check for duplicate mountpoint — add numeric suffix if needed
    char final_mp[64];
    str_copy(final_mp, mountpoint, 64);

    int dup_count = 0;
    for (uint32 i = 0; i < mount_count; i++) {
        if (mounts[i].active) {
            // Exact match or prefix match with digit suffix
            uint32 mplen = str_len(mountpoint);
            if (str_starts_with(mounts[i].mountpoint, mountpoint)) {
                char after = mounts[i].mountpoint[mplen];
                if (after == '\0' || (after >= '0' && after <= '9'))
                    dup_count++;
            }
        }
    }
    if (dup_count > 0) {
        // Append numeric suffix: /mnt/MYOS -> /mnt/MYOS2
        uint32 len = str_len(final_mp);
        if (len < 62) {
            final_mp[len] = '0' + (uint8)(dup_count + 1);
            final_mp[len + 1] = '\0';
        }
    }

    VFSMount *m = &mounts[mount_count];
    str_copy(m->mountpoint, final_mp, 64);
    m->fs_type = fs_type;
    m->device = device;
    m->active = 1;
    mount_count++;

    kprint("VFS: mounted ");
    kprint(m->fs_type == FS_TYPE_ISO9660 ? "iso9660" : "fat32");
    kprint(" at ");
    kprint(m->mountpoint);
    kprint("\n");

    return 0;
}

// Find the mount that best matches a path (longest mountpoint prefix)
static VFSMount *find_mount(const char *path, const char **remainder) {
    VFSMount *best = NULL;
    uint32 best_len = 0;

    for (uint32 i = 0; i < mount_count; i++) {
        if (!mounts[i].active) continue;
        uint32 mplen = str_len(mounts[i].mountpoint);

        // Check if path starts with mountpoint
        if (str_starts_with(path, mounts[i].mountpoint)) {
            char after = path[mplen];
            // Must match exactly or be followed by '/' or end
            if (after == '\0' || after == '/' || mplen == 1) {
                if (mplen > best_len) {
                    best = &mounts[i];
                    best_len = mplen;
                }
            }
        }
    }

    if (best && remainder) {
        const char *r = path + best_len;
        while (*r == '/') r++;
        *remainder = r;
    }

    return best;
}

void vfs_ls(const char *path) {
    const char *remainder;
    VFSMount *m = find_mount(path, &remainder);

    if (!m) {
        kprint("VFS: no mount for path\n");
        return;
    }

    if (m->fs_type == FS_TYPE_ISO9660) {
        // Pass the full sub-path to iso9660
        // If remainder is empty, list root of the ISO
        if (*remainder == '\0')
            iso9660_ls("/");
        else {
            char sub[128];
            sub[0] = '/';
            str_copy(sub + 1, remainder, 127);
            iso9660_ls(sub);
        }
    } else if (m->fs_type == FS_TYPE_FAT32) {
        // FAT32 only supports root listing currently
        fat32_list_root();
    }
}

int vfs_read_file(const char *path, void *buf, uint32 max_size) {
    const char *remainder;
    VFSMount *m = find_mount(path, &remainder);

    if (!m) return -1;

    if (m->fs_type == FS_TYPE_ISO9660) {
        char sub[128];
        sub[0] = '/';
        str_copy(sub + 1, remainder, 127);
        return iso9660_read_file(sub, buf, max_size);
    } else if (m->fs_type == FS_TYPE_FAT32) {
        // FAT32 cat takes just a filename (root-only)
        return fat32_cat_file(remainder);
    }

    return -1;
}

uint32 vfs_file_size(const char *path) {
    const char *remainder;
    VFSMount *m = find_mount(path, &remainder);

    if (!m) return 0;

    if (m->fs_type == FS_TYPE_ISO9660) {
        char sub[128];
        sub[0] = '/';
        str_copy(sub + 1, remainder, 127);
        return iso9660_file_size(sub);
    }
    // FAT32 doesn't have a file_size API currently
    return 0;
}

uint32 vfs_mount_count(void) {
    return mount_count;
}

const VFSMount *vfs_get_mount(uint32 idx) {
    if (idx >= mount_count) return NULL;
    return &mounts[idx];
}
