#include <shell/shell.h>
#include <drivers/monitor.h>
#include <arch/ports.h>
#include <kernel/mem.h>
#include <drivers/ide.h>
#include <fs/fat32.h>
#include <arch/acpi.h>
#include <kernel/cpu.h>
#include <drivers/pci.h>
#include <drivers/ahci.h>
#include <fs/vfs.h>
#include <net/nic.h>
#include <syscall.h>

// Command buffer
#define CMD_BUFFER_SIZE 80
static char cmd_buffer[CMD_BUFFER_SIZE];
static uint8 cmd_index = 0;

// Command table for tab completion
static const char *commands[] = {
    "help",
    "clear",
    "echo",
    "sys.reboot",
    "sys.cpu.ls",
    "sys.cpu.ring",
    "sys.mem.info",
    "sys.mem.free",
    "sys.mem.test",
    "sys.acpi.ls",
    "sys.disk.ls",
    "sys.disk.info",
    "sys.disk.read",
    "sys.disk.write",
    "sys.fs.ls",
    "sys.fs.cat",
    "sys.pci.ls",
    "sys.nic.ls",
    "sys.nic.mode",
    "sys.thread.ls",
    "sys.test.ap",
};
#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))

// External symbols for memory info
extern uint8 data_counter_mmap_entries;
extern uint32 MEMMAP_START;
extern uint32 PML4T_LOCATION;
extern uint32 PAGING_LOCATION_END;
extern uint32 mem_amount[2];

// String utilities
int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n > 0 && *s1 && (*s1 == *s2)) { s1++; s2++; n--; }
    if (n == 0) return 0;
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

size_t strlen(const char *s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

int starts_with(const char *str, const char *prefix) {
    while (*prefix) {
        if (*str != *prefix) return 0;
        str++; prefix++;
    }
    return 1;
}

// ============================================================================
// Output abstraction: direct kernel calls (ring 0) or syscall wrappers (ring 3)
// When running in ring 0 (boot), use direct calls.
// When running in ring 3 (after task_init), use syscall wrappers.
// ============================================================================
static uint8 use_syscalls = 0;

static void sh_putc(char c) {
    if (use_syscalls) sys_putc(c);
    else putc(c);
}

static void sh_print(char *s) {
    if (use_syscalls) sys_kprint(s);
    else kprint(s);
}

static void sh_cls(void) {
    if (use_syscalls) sys_cls();
    else cls();
}

static void sh_print_dec(uint64 n) {
    if (use_syscalls) sys_kprint_dec(n);
    else kprint_dec(n);
}

static void sh_print_hex(long n, char *post) {
    if (use_syscalls) sys_kprint_hex(n, post);
    else kprint_long2hex(n, post);
}

static void sh_print_dec_pad(uint64 n, uint32 w) {
    if (use_syscalls) sys_kprint_dec_pad(n, w);
    else kprint_dec_pad(n, w);
}

static void *sh_malloc(size_t sz) {
    if (use_syscalls) return sys_kmalloc(sz);
    else return kmalloc(sz);
}

static void sh_free(void *p) {
    if (use_syscalls) sys_kfree(p);
    else kfree(p);
}

static void sh_heap_stats(uint32 *u, uint32 *f, uint32 *t) {
    if (use_syscalls) sys_heap_stats(u, f, t);
    else heap_stats(u, f, t);
}

static int sh_ide_read(uint32 lba, uint8 cnt, uint8 *buf) {
    if (use_syscalls) return sys_ide_read(lba, cnt, buf);
    else return ide_read_sectors(lba, cnt, buf);
}

static int sh_ide_write(uint32 lba, uint8 *buf) {
    if (use_syscalls) return sys_ide_write(lba, buf);
    else return ide_write_sector(lba, buf);
}

static char *sh_ide_model(void) {
    if (use_syscalls) return sys_ide_model();
    else return ide_get_model();
}

static uint32 sh_ide_sectors(void) {
    if (use_syscalls) return sys_ide_sectors();
    else return ide_get_sector_count();
}

static void sh_fat32_ls(void) {
    if (use_syscalls) sys_fat32_ls();
    else fat32_list_root();
}

static int sh_fat32_cat(const char *n) {
    if (use_syscalls) return sys_fat32_cat(n);
    else return fat32_cat_file(n);
}

static void sh_acpi_ls(void) {
    if (use_syscalls) sys_acpi_ls();
    else acpi_lsacpi();
}

// ============================================================================
// Shell core
// ============================================================================

void shell_init() {
    sh_print("\n");
    sh_print("Welcome!\n");
    sh_print("Type 'help' for available commands.\n\n");
    cmd_index = 0;
    cmd_buffer[0] = '\0';
    shell_prompt();
}

void shell_prompt() {
    sh_print("> ");
}

static void shell_tab_complete(void) {
    if (cmd_index == 0) return;

    const char *first_match = NULL;
    uint32 match_count = 0;
    uint32 common_len = 0;

    for (uint32 i = 0; i < NUM_COMMANDS; i++) {
        if (starts_with(commands[i], cmd_buffer)) {
            match_count++;
            if (first_match == NULL) {
                first_match = commands[i];
                common_len = strlen(first_match);
            } else {
                uint32 j = cmd_index;
                while (j < common_len && first_match[j] == commands[i][j])
                    j++;
                common_len = j;
            }
        }
    }

    if (match_count == 0) return;

    if (common_len > cmd_index) {
        for (uint32 i = cmd_index; i < common_len && cmd_index < CMD_BUFFER_SIZE - 1; i++) {
            char ch = first_match[i];
            cmd_buffer[cmd_index++] = ch;
            sh_putc(ch);
        }
        cmd_buffer[cmd_index] = '\0';
        if (match_count == 1 && cmd_index < CMD_BUFFER_SIZE - 1) {
            cmd_buffer[cmd_index++] = ' ';
            cmd_buffer[cmd_index] = '\0';
            sh_putc(' ');
        }
    } else if (match_count > 1) {
        sh_putc('\n');
        for (uint32 i = 0; i < NUM_COMMANDS; i++) {
            if (starts_with(commands[i], cmd_buffer)) {
                sh_print("  ");
                sh_print((char *)commands[i]);
                sh_putc('\n');
            }
        }
        shell_prompt();
        for (uint8 i = 0; i < cmd_index; i++)
            sh_putc(cmd_buffer[i]);
    }
}

void shell_handle_char(char c) {
    if (c == 0x0A) {
        sh_putc('\n');
        shell_execute_command();
        cmd_index = 0;
        cmd_buffer[0] = '\0';
        shell_prompt();
    }
    else if (c == 0x09) {
        shell_tab_complete();
    }
    else if (c == 0x08) {
        if (cmd_index > 0) {
            cmd_index--;
            cmd_buffer[cmd_index] = '\0';
            sh_putc(0x08);
        }
    }
    else if (cmd_index < CMD_BUFFER_SIZE - 1) {
        cmd_buffer[cmd_index++] = c;
        cmd_buffer[cmd_index] = '\0';
        sh_putc(c);
    }
}

// ============================================================================
// Command handlers
// ============================================================================

void cmd_help() {
    sh_print("Available commands:\n");
    sh_print("  help            - Show this help message\n");
    sh_print("  clear           - Clear the screen\n");
    sh_print("  echo            - Echo text to screen\n");
    sh_print("  sys.reboot      - Reboot the system\n");
    sh_print("  sys.cpu.ls      - Show CPU and NUMA topology\n");
    sh_print("  sys.cpu.ring    - Show current privilege level\n");
    sh_print("  sys.mem.info    - Display memory information\n");
    sh_print("  sys.mem.free    - Show memory usage\n");
    sh_print("  sys.mem.test    - Test heap allocator\n");
    sh_print("  sys.acpi.ls     - List ACPI tables\n");
    sh_print("  sys.disk.ls     - List block devices\n");
    sh_print("  sys.disk.info   - Show disk information\n");
    sh_print("  sys.disk.read   - Read disk sectors\n");
    sh_print("  sys.disk.write  - Write disk sectors\n");
    sh_print("  sys.fs.ls [p]   - List directory (/ = ISO root, /mnt/* = FAT32)\n");
    sh_print("  sys.fs.cat <p>  - Print file contents\n");
    sh_print("  sys.pci.ls      - List PCI devices\n");
    sh_print("  sys.nic.ls      - List network interfaces\n");
    sh_print("  sys.nic.mode    - Show or set NIC assignment mode (per-numa|per-core)\n");
    sh_print("  sys.thread.ls   - Show per-CPU thread metadata (NUMA, NIC)\n");
}

void cmd_clear() {
    sh_cls();
}

void cmd_meminfo() {
    if (use_syscalls) {
        SysMemInfo info;
        sys_meminfo(&info);
        sh_print("Memory Information:\n");
        sh_print_hex(info.mmap_entries, " MMAP entries\n");
        sh_print_hex(info.paging_location_end, " Paging structures end\n");
    } else {
        sh_print("Memory Information:\n");
        sh_print_hex(data_counter_mmap_entries, " MMAP entries\n");
        sh_print_hex((long)&MEMMAP_START, " MEMMAP start address\n");
        sh_print_hex(PML4T_LOCATION, " PML4T location\n");
        sh_print_hex(PAGING_LOCATION_END, " Paging structures end\n");
    }
}

void cmd_memtest() {
    sh_print("Testing heap allocator...\n");

    void *p1 = sh_malloc(100);
    if (p1) {
        sh_print("Allocated block 1: ");
        sh_print_hex((uint64)p1, "\n");
    } else {
        sh_print("Failed to allocate block 1\n");
        return;
    }

    void *p2 = sh_malloc(200);
    if (p2) {
        sh_print("Allocated block 2: ");
        sh_print_hex((uint64)p2, "\n");
    } else {
        sh_print("Failed to allocate block 2\n");
        sh_free(p1);
        return;
    }

    uint32 *test = (uint32*)p1;
    *test = 0xDEADBEEF;
    sh_print("Write test: ");
    sh_print_hex(*test, "\n");

    sh_free(p1);
    sh_print("Freed block 1\n");

    void *p3 = sh_malloc(50);
    if (p3) {
        sh_print("Allocated block 3: ");
        sh_print_hex((uint64)p3, "\n");
    }

    sh_free(p2);
    sh_free(p3);
    sh_print("Test complete\n");
}

void cmd_lsblk() {
    sh_print("NAME    TYPE     SIZE       MODEL\n");
    int found = 0;

    // IDE devices
    uint32 sectors = sh_ide_sectors();
    if (sectors > 0) {
        uint32 size_mb = sectors / 2048;
        sh_print("hda     disk     ");
        sh_print_dec(size_mb);
        sh_print(" MB     ");
        sh_print(sh_ide_model());
        sh_putc('\n');
        found++;
    }

    // AHCI devices
    uint32 ahci_count = ahci_device_count();
    for (uint32 i = 0; i < ahci_count; i++) {
        const AHCIDevice *dev = ahci_get_device(i);
        if (!dev || !dev->present) continue;
        sh_print("sata");
        sh_print_dec(i);
        if (dev->type == AHCI_DEV_SATAPI)
            sh_print("   cdrom    ");
        else {
            sh_print("   disk     ");
        }
        if (dev->sector_count > 0) {
            uint64 size_mb = (dev->sector_count * dev->sector_size) / (1024 * 1024);
            sh_print_dec(size_mb);
            sh_print(" MB     ");
        } else {
            sh_print("-          ");
        }
        sh_print(dev->model[0] ? (char *)dev->model : "?");
        sh_putc('\n');
        found++;
    }

    if (found == 0)
        sh_print("No drives detected\n");
}

void cmd_diskinfo() {
    sh_print("Disk Information:\n");
    uint32 sectors = sh_ide_sectors();
    if (sectors == 0) {
        sh_print("No drive detected\n");
        return;
    }
    sh_print("Drive: Primary Master\n");
    sh_print("Model: ");
    sh_print(sh_ide_model());
    sh_putc('\n');
    sh_print("Sectors: ");
    sh_print_dec(sectors);
    sh_putc('\n');
    uint32 size_mb = sectors / 2048;
    sh_print("Size: ");
    sh_print_dec(size_mb);
    sh_print(" MB\n");
}

static uint32 parse_number(const char *str) {
    uint32 result = 0;
    if (str[0] == '0' && str[1] == 'x') {
        str += 2;
        while (*str) {
            char c = *str;
            if (c >= '0' && c <= '9') result = result * 16 + (c - '0');
            else if (c >= 'a' && c <= 'f') result = result * 16 + (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') result = result * 16 + (c - 'A' + 10);
            else break;
            str++;
        }
    } else {
        while (*str >= '0' && *str <= '9') {
            result = result * 10 + (*str - '0');
            str++;
        }
    }
    return result;
}

static void hex_dump(uint8 *data, uint32 len) {
    for (uint32 i = 0; i < len; i++) {
        if (i % 16 == 0) {
            if (i != 0) sh_putc('\n');
            sh_print_hex(i, ": ");
        }
        uint8 b = data[i];
        sh_putc("0123456789ABCDEF"[b >> 4]);
        sh_putc("0123456789ABCDEF"[b & 0xF]);
        sh_putc(' ');
    }
    sh_putc('\n');
}

void cmd_diskread() {
    char *args = cmd_buffer + 13;
    while (*args == ' ') args++;
    if (*args == '\0') {
        sh_print("Usage: sys.disk.read <lba> [count]\n");
        return;
    }
    uint32 lba = parse_number(args);
    while (*args && *args != ' ') args++;
    while (*args == ' ') args++;
    uint32 count = (*args) ? parse_number(args) : 1;
    if (count > 8) count = 8;

    uint8 *buffer = (uint8*)sh_malloc(count * 512);
    if (!buffer) {
        sh_print("Failed to allocate buffer\n");
        return;
    }
    sh_print("Reading ");
    sh_print_hex(count, " sector(s) from LBA ");
    sh_print_hex(lba, "\n");

    if (sh_ide_read(lba, count, buffer) != 0) {
        sh_print("Read failed\n");
        sh_free(buffer);
        return;
    }
    hex_dump(buffer, count * 512);
    sh_free(buffer);
}

void cmd_diskwrite() {
    char *args = cmd_buffer + 14;
    while (*args == ' ') args++;
    if (*args == '\0') {
        sh_print("Usage: sys.disk.write <lba> <data>\n");
        return;
    }
    uint32 lba = parse_number(args);
    while (*args && *args != ' ') args++;
    while (*args == ' ') args++;
    if (*args == '\0') {
        sh_print("No data specified\n");
        return;
    }

    uint8 *buffer = (uint8*)sh_malloc(512);
    if (!buffer) {
        sh_print("Failed to allocate buffer\n");
        return;
    }
    for (int i = 0; i < 512; i++) buffer[i] = 0;
    int len = 0;
    while (args[len] && len < 512) {
        buffer[len] = args[len];
        len++;
    }
    sh_print("Writing to LBA ");
    sh_print_hex(lba, "\n");
    if (sh_ide_write(lba, buffer) != 0)
        sh_print("Write failed\n");
    else
        sh_print("Write complete\n");
    sh_free(buffer);
}

void cmd_free() {
    uint32 used, free_pages, total;
    sh_heap_stats(&used, &free_pages, &total);

    uint64 total_bytes, total_mb;
    uint32 numa_nodes;
    uint64 paging_end;

    if (use_syscalls) {
        SysMemInfo minfo;
        sys_meminfo(&minfo);
        total_bytes = (minfo.mem_amount_high << 32) | minfo.mem_amount_low;
        paging_end = minfo.paging_location_end;
        SysCpuInfo cinfo;
        sys_cpu_info(&cinfo);
        numa_nodes = cinfo.numa_node_count;
    } else {
        total_bytes = ((uint64)mem_amount[0] << 32) | (uint64)mem_amount[1];
        paging_end = (uint64)PAGING_LOCATION_END;
        numa_nodes = acpi_numa_node_count();
    }

    total_mb = total_bytes / (1024 * 1024);
    uint32 heap_used_kb = used * 4;
    uint32 heap_free_kb = free_pages * 4;
    uint32 heap_total_kb = total * 4;

    if (numa_nodes == 0) numa_nodes = 1;
    uint64 per_node_mb = total_mb / numa_nodes;

    uint64 kernel_kb = paging_end / 1024;
    uint64 node0_used_kb = kernel_kb + heap_used_kb;
    uint64 node0_free_mb = per_node_mb - (node0_used_kb / 1024);

    sh_print("              total       used       free\n");

    sh_print("Cluster ");
    sh_print_dec_pad(total_mb, 10);
    sh_print(" MB");
    sh_print_dec_pad(node0_used_kb / 1024, 9);
    sh_print(" MB");
    sh_print_dec_pad(total_mb - (node0_used_kb / 1024), 9);
    sh_print(" MB\n");

    sh_print("Node    ");
    sh_print_dec_pad(total_mb, 10);
    sh_print(" MB");
    sh_print_dec_pad(node0_used_kb / 1024, 9);
    sh_print(" MB");
    sh_print_dec_pad(total_mb - (node0_used_kb / 1024), 9);
    sh_print(" MB\n");

    sh_print("NUMA 0  ");
    sh_print_dec_pad(per_node_mb, 10);
    sh_print(" MB");
    sh_print_dec_pad(node0_used_kb / 1024, 9);
    sh_print(" MB");
    sh_print_dec_pad(node0_free_mb, 9);
    sh_print(" MB\n");

    if (numa_nodes > 1) {
        sh_print("NUMA 1  ");
        sh_print_dec_pad(per_node_mb, 10);
        sh_print(" MB");
        sh_print_dec_pad(0, 9);
        sh_print(" MB");
        sh_print_dec_pad(per_node_mb, 9);
        sh_print(" MB\n");
    }

    sh_print("Heap    ");
    sh_print_dec_pad(heap_total_kb, 10);
    sh_print(" KB");
    sh_print_dec_pad(heap_used_kb, 9);
    sh_print(" KB");
    sh_print_dec_pad(heap_free_kb, 9);
    sh_print(" KB\n");
}

void cmd_echo() {
    char *text = cmd_buffer + 4;
    while (*text == ' ') text++;
    if (*text) {
        sh_print(text);
        sh_putc('\n');
    }
}

void cmd_ring() {
    uint16 cs;
    __asm__ volatile("mov %%cs, %0" : "=r"(cs));
    uint8 cpl = cs & 0x3;
    sh_print("Current privilege level: ring ");
    sh_print_dec(cpl);
    sh_print("\n");
    sh_print("CS: ");
    sh_print_hex(cs, "\n");
}

void cmd_lscpu() {
    if (use_syscalls) {
        SysCpuInfo info;
        sys_cpu_info(&info);

        sh_print("CPU(s):             ");
        sh_print_dec(info.cpu_count);
        sh_print("\n");

        uint32 online = 0;
        for (uint32 i = 0; i < info.cpu_count; i++) {
            if (info.cpus[i].running) online++;
        }
        sh_print("Online CPU(s):      ");
        sh_print_dec(online);
        sh_print("\n");

        sh_print("LAPIC base:         ");
        sh_print_hex(info.lapic_base, "\n");
        sh_print("IOAPIC base:        ");
        sh_print_hex(info.ioapic_base, "\n");

        uint32 numa_nodes = info.numa_node_count;
        if (numa_nodes == 0) numa_nodes = 1;
        sh_print("\nNUMA node(s):       ");
        sh_print_dec(numa_nodes);
        sh_print("\n");

        sh_print("\nPer-CPU info:\n");
        for (uint32 i = 0; i < info.cpu_count; i++) {
            sh_print("  CPU ");
            sh_print_dec(i);
            sh_print("  LAPIC ");
            sh_print_dec(info.cpus[i].lapic_id);
            if (info.cpus[i].has_numa) {
                sh_print("  NUMA ");
                sh_print_dec(info.cpus[i].numa_node);
            }
            if (info.cpus[i].running)
                sh_print("  [online]");
            else
                sh_print("  [offline]");
            if (i == 0)
                sh_print("  (BSP)");
            sh_print("\n");
        }
    } else {
        sh_print("CPU(s):             ");
        sh_print_dec(cpu_count);
        sh_print("\n");

        uint32 online = 0;
        for (uint32 i = 0; i < cpu_count; i++) {
            if (percpu[i].running) online++;
        }
        sh_print("Online CPU(s):      ");
        sh_print_dec(online);
        sh_print("\n");

        sh_print("LAPIC base:         ");
        sh_print_hex(lapic_base_addr, "\n");
        sh_print("IOAPIC base:        ");
        sh_print_hex(ioapic_base_addr, "\n");

        uint32 numa_nodes = acpi_numa_node_count();
        if (numa_nodes == 0) numa_nodes = 1;
        sh_print("\nNUMA node(s):       ");
        sh_print_dec(numa_nodes);
        sh_print("\n");

        sh_print("\nPer-CPU info:\n");
        for (uint32 i = 0; i < cpu_count; i++) {
            uint32 node = 0;
            int has_node = acpi_cpu_to_node(percpu[i].lapic_id, &node);
            sh_print("  CPU ");
            sh_print_dec(i);
            sh_print("  LAPIC ");
            sh_print_dec(percpu[i].lapic_id);
            if (has_node) {
                sh_print("  NUMA ");
                sh_print_dec(node);
            }
            if (percpu[i].running)
                sh_print("  [online]");
            else
                sh_print("  [offline]");
            if (i == 0)
                sh_print("  (BSP)");
            sh_print("\n");
        }
    }
}

// cmd_reboot runs in ring 0 via SYS_REBOOT syscall handler
void cmd_reboot() {
    sh_print("Rebooting system...\n");
    __asm__ __volatile__("cli");

    ACPIResetRegInfo reset;
    if (acpi_reset_reg_info(&reset)) {
        if (reset.space_id == 1 && reset.address <= 0xFFFF) {
            uint16 port = (uint16)reset.address;
            if (reset.access_size == 2)
                outw(port, (uint16)reset.value);
            else
                outb(port, reset.value);
        } else if (reset.space_id == 0 && reset.address != 0) {
            volatile uint8 *mmio = (volatile uint8 *)(uint64)reset.address;
            *mmio = reset.value;
        }
        for (volatile int i = 0; i < 100000; i++);
    }

    outb(0xCF9, 0x00);
    outb(0xCF9, 0x02);
    outb(0xCF9, 0x06);
    for (volatile int i = 0; i < 100000; i++);

    uint8 temp;
    int timeout = 10000;
    do {
        temp = inb(0x64);
        if (--timeout == 0) break;
    } while (temp & 0x02);
    outb(0x64, 0xFE);
    for (volatile int i = 0; i < 100000; i++);

    struct {
        uint16 limit;
        uint64 base;
    } __attribute__((packed)) idtr = { 0, 0 };
    __asm__ __volatile__("lidt %0" : : "m"(idtr));
    __asm__ __volatile__("int $0x03");

    while (1) { __asm__ __volatile__("hlt"); }
}

static const char hex_tbl[] = "0123456789abcdef";

static void sh_print_hex16(uint16 val) {
    sh_putc(hex_tbl[(val >> 12) & 0xF]);
    sh_putc(hex_tbl[(val >> 8) & 0xF]);
    sh_putc(hex_tbl[(val >> 4) & 0xF]);
    sh_putc(hex_tbl[val & 0xF]);
}

static void sh_print_hex8(uint8 val) {
    sh_putc(hex_tbl[(val >> 4) & 0xF]);
    sh_putc(hex_tbl[val & 0xF]);
}

void cmd_lspci() {
    uint32 count = pci_get_device_count();
    if (count == 0) {
        sh_print("No PCI devices found\n");
        return;
    }
    for (uint32 i = 0; i < count; i++) {
        const PCIDevice *d = pci_get_device(i);
        sh_print_hex8(d->bus);
        sh_putc(':');
        sh_print_hex8(d->dev);
        sh_putc('.');
        sh_print_dec(d->func);
        sh_print("  ");
        sh_print_hex16(d->vendor_id);
        sh_putc(':');
        sh_print_hex16(d->device_id);
        sh_print("  class ");
        sh_print_hex8(d->class_code);
        sh_putc(':');
        sh_print_hex8(d->subclass);
        sh_print("  NUMA ");
        if (d->numa_node == PCI_NUMA_UNKNOWN)
            sh_putc('-');
        else
            sh_print_dec(d->numa_node);
        const char *vname = pci_vendor_name(d->vendor_id);
        const char *cname = pci_class_name(d->class_code, d->subclass);
        sh_print("  ");
        sh_print(vname ? (char *)vname : "?");
        sh_print("  ");
        sh_print(cname ? (char *)cname : "?");
        sh_putc('\n');
    }
}

static void print_mode(NicAssignmentMode m) {
    sh_print(m == NIC_MODE_PER_CORE ? "per-core" : "per-numa");
}

void cmd_nic_mode() {
    char *args = cmd_buffer + strlen("sys.nic.mode");
    while (*args == ' ') args++;
    if (*args == '\0') {
        sh_print("mode: ");
        print_mode(nic_get_mode());
        sh_putc('\n');
        return;
    }
    if (strcmp(args, "per-numa") == 0) {
        nic_set_mode(NIC_MODE_PER_NUMA);
        nic_assign();
        sh_print("mode: per-numa\n");
    } else if (strcmp(args, "per-core") == 0) {
        nic_set_mode(NIC_MODE_PER_CORE);
        nic_assign();
        sh_print("mode: per-core\n");
    } else {
        sh_print("Usage: sys.nic.mode [per-numa|per-core]\n");
    }
}

void cmd_thread_ls() {
    sh_print("mode: ");
    print_mode(nic_get_mode());
    sh_putc('\n');
    sh_print("CPU  NUMA  NIC          PCI         MAC\n");
    for (uint32 i = 0; i < cpu_count; i++) {
        ThreadMeta *tm = thread_meta_get(i);
        sh_print_dec_pad(i, 3);
        sh_print("  ");
        if (tm->numa_node == THREAD_NUMA_UNKNOWN) {
            sh_print("  -");
        } else {
            sh_print_dec_pad(tm->numa_node, 3);
        }
        sh_print("  ");
        if (tm->nic_index == NIC_NONE) {
            sh_print("(none)       ");
            sh_print("           ");
            sh_print("                 ");
        } else {
            const char *name = nic_name(tm->nic_index);
            sh_print(name ? (char *)name : "?");
            // pad name to 13 chars
            int nlen = 0;
            if (name) { while (name[nlen] && nlen < 13) nlen++; }
            for (int p = nlen; p < 13; p++) sh_putc(' ');
            sh_print_hex8(tm->nic_bus);
            sh_putc(':');
            sh_print_hex8(tm->nic_dev);
            sh_putc('.');
            sh_print_dec(tm->nic_func);
            sh_print("    ");
            for (int j = 0; j < 6; j++) {
                sh_print_hex8(tm->nic_mac[j]);
                if (j < 5) sh_putc(':');
            }
        }
        sh_putc('\n');
    }
}

void cmd_lsnic() {
    uint32 count = nic_get_count();
    if (count == 0) {
        sh_print("No NICs found\n");
        return;
    }
    for (uint32 i = 0; i < count; i++) {
        sh_print((char *)nic_name(i));
        sh_print("  NUMA ");
        uint32 node = nic_get_numa_node(i);
        if (node == PCI_NUMA_UNKNOWN)
            sh_putc('-');
        else
            sh_print_dec(node);
        sh_print("  MAC ");
        uint8 mac[6];
        nic_get_mac(i, mac);
        for (int j = 0; j < 6; j++) {
            sh_print_hex8(mac[j]);
            if (j < 5) sh_putc(':');
        }
        sh_print(nic_link_status(i) ? "  link up" : "  link down");
        sh_putc('\n');
    }
}

void shell_execute_command() {
    while (cmd_index > 0 && cmd_buffer[cmd_index - 1] == ' ') {
        cmd_index--;
        cmd_buffer[cmd_index] = '\0';
    }
    if (cmd_buffer[0] == '\0') return;

    if (strcmp(cmd_buffer, "help") == 0) cmd_help();
    else if (strcmp(cmd_buffer, "clear") == 0) cmd_clear();
    else if (starts_with(cmd_buffer, "echo")) cmd_echo();
    else if (strcmp(cmd_buffer, "sys.reboot") == 0) {
        if (use_syscalls) sys_reboot();
        else cmd_reboot();
    }
    else if (strcmp(cmd_buffer, "sys.cpu.ls") == 0) cmd_lscpu();
    else if (strcmp(cmd_buffer, "sys.cpu.ring") == 0) cmd_ring();
    else if (strcmp(cmd_buffer, "sys.mem.info") == 0) cmd_meminfo();
    else if (strcmp(cmd_buffer, "sys.mem.free") == 0) cmd_free();
    else if (strcmp(cmd_buffer, "sys.mem.test") == 0) cmd_memtest();
    else if (strcmp(cmd_buffer, "sys.acpi.ls") == 0) sh_acpi_ls();
    else if (strcmp(cmd_buffer, "sys.disk.ls") == 0) cmd_lsblk();
    else if (strcmp(cmd_buffer, "sys.disk.info") == 0) cmd_diskinfo();
    else if (starts_with(cmd_buffer, "sys.disk.read")) cmd_diskread();
    else if (starts_with(cmd_buffer, "sys.disk.write")) cmd_diskwrite();
    else if (starts_with(cmd_buffer, "sys.fs.ls")) {
        char *args = cmd_buffer + 9;
        while (*args == ' ') args++;
        if (use_syscalls)
            sys_iso_ls(*args ? args : "/");
        else
            vfs_ls(*args ? args : "/");
    }
    else if (starts_with(cmd_buffer, "sys.fs.cat")) {
        char *args = cmd_buffer + 10;
        while (*args == ' ') args++;
        if (*args == '\0') {
            sh_print("Usage: sys.fs.cat <path>\n");
        } else {
            uint8 *buf = (uint8 *)sh_malloc(65536);
            if (!buf) {
                sh_print("Out of memory\n");
            } else {
                int rd;
                if (use_syscalls)
                    rd = sys_iso_read(args, buf, 65536);
                else
                    rd = vfs_read_file(args, buf, 65536);
                if (rd > 0) {
                    for (int i = 0; i < rd; i++)
                        sh_putc((char)buf[i]);
                    sh_putc('\n');
                } else {
                    sh_print("File not found\n");
                }
                sh_free(buf);
            }
        }
    }
    else if (strcmp(cmd_buffer, "sys.pci.ls") == 0) cmd_lspci();
    else if (strcmp(cmd_buffer, "sys.nic.ls") == 0) cmd_lsnic();
    else if (starts_with(cmd_buffer, "sys.nic.mode")) cmd_nic_mode();
    else if (strcmp(cmd_buffer, "sys.thread.ls") == 0) cmd_thread_ls();
    else if (strcmp(cmd_buffer, "sys.test.ap") == 0) {
        if (use_syscalls) sys_test_ap();
    }
    else {
        sh_print("Unknown command: ");
        sh_print(cmd_buffer);
        sh_putc('\n');
    }
}

// Ring 3 entry point -- sets use_syscalls flag and runs the shell loop
void shell_ring3_entry(void) {
    use_syscalls = 1;
    shell_init();
    while (1) {
        int c = sys_wait_input();
        if (c != 0)
            shell_handle_char((char)c);
    }
}
