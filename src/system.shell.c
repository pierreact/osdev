#include <system.shell.h>
#include <system.monitor.h>
#include <system.ports.h>
#include <system.mem.h>
#include <system.ide.h>
#include <system.fat32.h>
#include <system.acpi.h>
#include <system.cpu.h>

// Command buffer
#define CMD_BUFFER_SIZE 80
static char cmd_buffer[CMD_BUFFER_SIZE];
static uint8 cmd_index = 0;

// External symbols for memory info
extern uint8 data_counter_mmap_entries;
extern uint32 MEMMAP_START;
extern uint32 PML4T_LOCATION;
extern uint32 PAGING_LOCATION_END;
extern uint32 mem_amount[2]; // [0] = high dword, [1] = low dword

// ============================================================================
// String utility functions (no stdlib in kernel)
// ============================================================================

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n > 0 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
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
        str++;
        prefix++;
    }
    return 1;
}

// ============================================================================
// Shell core functions
// ============================================================================

void shell_init() {
    kprint("\n");
    kprint("Welcome!\n");
    kprint("Type 'help' for available commands.\n\n");
    cmd_index = 0;
    cmd_buffer[0] = '\0';
    shell_prompt();
}

void shell_prompt() {
    kprint("> ");
}

void shell_handle_char(char c) {
    if (c == 0x0A) {  // Enter key
        putc('\n');
        shell_execute_command();
        cmd_index = 0;
        cmd_buffer[0] = '\0';
        shell_prompt();
    } 
    else if (c == 0x08) {  // Backspace
        if (cmd_index > 0) {
            cmd_index--;
            cmd_buffer[cmd_index] = '\0';
            putc(0x08);  // Send backspace to display
        }
    }
    else if (cmd_index < CMD_BUFFER_SIZE - 1) {
        cmd_buffer[cmd_index++] = c;
        cmd_buffer[cmd_index] = '\0';
        putc(c);  // Echo character
    }
}

// ============================================================================
// Command handlers
// ============================================================================

void cmd_help() {
    kprint("Available commands:\n");
    kprint("  help      - Show this help message\n");
    kprint("  clear     - Clear the screen\n");
    kprint("  meminfo   - Display memory information\n");
    kprint("  memtest   - Test heap allocator\n");
    kprint("  lsblk     - List block devices\n");
    kprint("  diskinfo  - Show disk information\n");
    kprint("  diskread  - Read disk sectors\n");
    kprint("  diskwrite - Write disk sectors\n");
    kprint("  ls        - List files on FAT32 disk\n");
    kprint("  cat <file>- Print file contents\n");
    kprint("  free      - Show memory usage\n");
    kprint("  lsacpi    - List ACPI tables\n");
    kprint("  lscpu     - Show CPU and NUMA topology\n");
    kprint("  echo      - Echo text to screen\n");
    kprint("  reboot    - Reboot the system\n");
}

void cmd_clear() {
    cls();
}

void cmd_meminfo() {
    kprint("Memory Information:\n");
    kprint_long2hex(data_counter_mmap_entries, " MMAP entries\n");
    kprint_long2hex((long)&MEMMAP_START, " MEMMAP start address\n");
    kprint_long2hex(PML4T_LOCATION, " PML4T location\n");
    kprint_long2hex(PAGING_LOCATION_END, " Paging structures end\n");
}

void cmd_memtest() {
    kprint("Testing heap allocator...\n");
    
    void *p1 = kmalloc(100);
    if(p1) {
        kprint("Allocated block 1: ");
        kprint_long2hex((uint64)p1, "\n");
    } else {
        kprint("Failed to allocate block 1\n");
        return;
    }
    
    void *p2 = kmalloc(200);
    if(p2) {
        kprint("Allocated block 2: ");
        kprint_long2hex((uint64)p2, "\n");
    } else {
        kprint("Failed to allocate block 2\n");
        kfree(p1);
        return;
    }
    
    uint32 *test = (uint32*)p1;
    *test = 0xDEADBEEF;
    kprint("Write test: ");
    kprint_long2hex(*test, "\n");
    
    kfree(p1);
    kprint("Freed block 1\n");
    
    void *p3 = kmalloc(50);
    if(p3) {
        kprint("Allocated block 3: ");
        kprint_long2hex((uint64)p3, "\n");
    }
    
    kfree(p2);
    kfree(p3);
    kprint("Test complete\n");
}

void cmd_lsblk() {
    kprint("NAME  SIZE    TYPE\n");
    
    uint32 sectors = ide_get_sector_count();
    if(sectors == 0) {
        kprint("No drives detected\n");
        return;
    }
    
    uint32 size_mb = (sectors / 2048);
    kprint("hda   ");
    kprint_dec(size_mb);
    kprint("MB  disk\n");
}

void cmd_diskinfo() {
    kprint("Disk Information:\n");
    
    uint32 sectors = ide_get_sector_count();
    if(sectors == 0) {
        kprint("No drive detected\n");
        return;
    }
    
    kprint("Drive: Primary Master\n");
    kprint("Model: ");
    kprint(ide_get_model());
    putc('\n');
    kprint("Sectors: ");
    kprint_dec(sectors);
    putc('\n');
    
    uint32 size_mb = (sectors / 2048);
    kprint("Size: ");
    kprint_dec(size_mb);
    kprint(" MB\n");
}

static uint32 parse_number(const char *str) {
    uint32 result = 0;
    
    if(str[0] == '0' && str[1] == 'x') {
        str += 2;
        while(*str) {
            char c = *str;
            if(c >= '0' && c <= '9') result = result * 16 + (c - '0');
            else if(c >= 'a' && c <= 'f') result = result * 16 + (c - 'a' + 10);
            else if(c >= 'A' && c <= 'F') result = result * 16 + (c - 'A' + 10);
            else break;
            str++;
        }
    } else {
        while(*str >= '0' && *str <= '9') {
            result = result * 10 + (*str - '0');
            str++;
        }
    }
    
    return result;
}

static void hex_dump(uint8 *data, uint32 len) {
    for(uint32 i = 0; i < len; i++) {
        if(i % 16 == 0) {
            if(i != 0) putc('\n');
            kprint_long2hex(i, ": ");
        }
        
        uint8 b = data[i];
        char hex[3];
        hex[0] = "0123456789ABCDEF"[b >> 4];
        hex[1] = "0123456789ABCDEF"[b & 0xF];
        hex[2] = ' ';
        
        putc(hex[0]);
        putc(hex[1]);
        putc(hex[2]);
    }
    putc('\n');
}

void cmd_diskread() {
    char *args = cmd_buffer + 8;
    while(*args == ' ') args++;
    
    if(*args == '\0') {
        kprint("Usage: diskread <lba> [count]\n");
        return;
    }
    
    uint32 lba = parse_number(args);
    
    while(*args && *args != ' ') args++;
    while(*args == ' ') args++;
    
    uint32 count = (*args) ? parse_number(args) : 1;
    if(count > 8) count = 8;
    
    uint8 *buffer = (uint8*)kmalloc(count * 512);
    if(!buffer) {
        kprint("Failed to allocate buffer\n");
        return;
    }
    
    kprint("Reading ");
    kprint_long2hex(count, " sector(s) from LBA ");
    kprint_long2hex(lba, "\n");
    
    if(ide_read_sectors(lba, count, buffer) != 0) {
        kprint("Read failed\n");
        kfree(buffer);
        return;
    }
    
    hex_dump(buffer, count * 512);
    kfree(buffer);
}

void cmd_diskwrite() {
    char *args = cmd_buffer + 9;
    while(*args == ' ') args++;
    
    if(*args == '\0') {
        kprint("Usage: diskwrite <lba> <data>\n");
        return;
    }
    
    uint32 lba = parse_number(args);
    
    while(*args && *args != ' ') args++;
    while(*args == ' ') args++;
    
    if(*args == '\0') {
        kprint("No data specified\n");
        return;
    }
    
    uint8 *buffer = (uint8*)kmalloc(512);
    if(!buffer) {
        kprint("Failed to allocate buffer\n");
        return;
    }
    
    for(int i = 0; i < 512; i++) buffer[i] = 0;
    
    int len = 0;
    while(args[len] && len < 512) {
        buffer[len] = args[len];
        len++;
    }
    
    kprint("Writing to LBA ");
    kprint_long2hex(lba, "\n");
    
    if(ide_write_sector(lba, buffer) != 0) {
        kprint("Write failed\n");
    } else {
        kprint("Write complete\n");
    }
    
    kfree(buffer);
}

// Display memory usage: cluster, node, per-NUMA, and heap breakdown
void cmd_free() {
    uint32 used, free_pages, total;
    heap_stats(&used, &free_pages, &total);

    // mem_amount is stored big-endian: [0]=high dword, [1]=low dword
    uint64 total_bytes = ((uint64)mem_amount[0] << 32) | (uint64)mem_amount[1];
    uint64 total_mb = total_bytes / (1024 * 1024);
    uint32 heap_used_kb = used * 4;       // Each block is 4KB
    uint32 heap_free_kb = free_pages * 4;
    uint32 heap_total_kb = total * 4;

    // NUMA: 2 nodes, split evenly (hardcoded until SRAT parsing)
    uint32 numa_nodes = 2;
    uint64 per_node_mb = total_mb / numa_nodes;

    // Kernel + paging structures consume low memory on node 0
    uint64 kernel_kb = (uint64)PAGING_LOCATION_END / 1024;
    uint64 node0_used_kb = kernel_kb + heap_used_kb;
    uint64 node0_free_mb = per_node_mb - (node0_used_kb / 1024);

    kprint("              total       used       free\n");

    // Cluster total (single node for now)
    kprint("Cluster ");
    kprint_dec_pad(total_mb, 10);
    kprint(" MB");
    kprint_dec_pad(node0_used_kb / 1024, 9);
    kprint(" MB");
    kprint_dec_pad(total_mb - (node0_used_kb / 1024), 9);
    kprint(" MB\n");

    // This node
    kprint("Node    ");
    kprint_dec_pad(total_mb, 10);
    kprint(" MB");
    kprint_dec_pad(node0_used_kb / 1024, 9);
    kprint(" MB");
    kprint_dec_pad(total_mb - (node0_used_kb / 1024), 9);
    kprint(" MB\n");

    // Per-NUMA node
    kprint("NUMA 0  ");
    kprint_dec_pad(per_node_mb, 10);
    kprint(" MB");
    kprint_dec_pad(node0_used_kb / 1024, 9);
    kprint(" MB");
    kprint_dec_pad(node0_free_mb, 9);
    kprint(" MB\n");

    kprint("NUMA 1  ");
    kprint_dec_pad(per_node_mb, 10);
    kprint(" MB");
    kprint_dec_pad(0, 9);
    kprint(" MB");
    kprint_dec_pad(per_node_mb, 9);
    kprint(" MB\n");

    // Heap detail
    kprint("Heap    ");
    kprint_dec_pad(heap_total_kb, 10);
    kprint(" KB");
    kprint_dec_pad(heap_used_kb, 9);
    kprint(" KB");
    kprint_dec_pad(heap_free_kb, 9);
    kprint(" KB\n");
}

void cmd_echo() {
    // Skip "echo" and any spaces
    char *text = cmd_buffer + 4;  // Skip "echo"
    while (*text == ' ') text++;
    
    if (*text) {
        kprint(text);
        putc('\n');
    }
}

// Display CPU topology: count, LAPIC/IOAPIC addresses, NUMA layout, per-CPU status
void cmd_lscpu() {
    kprint("CPU(s):             ");
    kprint_dec(cpu_count);
    kprint("\n");

    // Count online APs
    uint32 online = 0;
    for (uint32 i = 0; i < cpu_count; i++) {
        if (percpu[i].running) online++;
    }
    kprint("Online CPU(s):      ");
    kprint_dec(online);
    kprint("\n");

    kprint("LAPIC base:         ");
    kprint_long2hex(lapic_base_addr, "\n");

    kprint("IOAPIC base:        ");
    kprint_long2hex(ioapic_base_addr, "\n");

    // Hardcoded until SRAT parsing gives us real topology
    kprint("\nNUMA node(s):       2\n");
    kprint("  NUMA node 0:      CPU 0-1\n");
    kprint("  NUMA node 1:      CPU 2-3\n");

    kprint("\nPer-CPU info:\n");
    for (uint32 i = 0; i < cpu_count; i++) {
        kprint("  CPU ");
        kprint_dec(i);
        kprint("  LAPIC ");
        kprint_dec(percpu[i].lapic_id);
        if (percpu[i].running)
            kprint("  [online]");
        else
            kprint("  [offline]");
        if (i == 0)
            kprint("  (BSP)");
        kprint("\n");
    }
}

void cmd_reboot() {
    kprint("Rebooting system...\n");
    
    // Disable interrupts
    __asm__ __volatile__("cli");
    
    // Method 1: PCI Reset (most reliable on modern systems)
    outb(0xCF9, 0x00);  // Clear reset control
    outb(0xCF9, 0x02);  // Request system reset
    outb(0xCF9, 0x06);  // Actually reset (bit 2=system reset, bit 1=reset CPU)
    
    // Short delay
    for(volatile int i = 0; i < 100000; i++);
    
    // Method 2: Keyboard controller reset
    uint8 temp;
    int timeout = 10000;
    do {
        temp = inb(0x64);
        if (--timeout == 0) break;
    } while (temp & 0x02);  // Wait until input buffer is empty
    
    outb(0x64, 0xFE);  // Send reset command
    
    // Short delay
    for(volatile int i = 0; i < 100000; i++);
    
    // Method 3: Triple fault (if everything else failed)
    // Load invalid IDT and cause interrupt
    struct {
        uint16 limit;
        uint64 base;
    } __attribute__((packed)) idtr = { 0, 0 };
    
    __asm__ __volatile__("lidt %0" : : "m"(idtr));
    __asm__ __volatile__("int $0x03");  // Trigger interrupt with null IDT
    
    // If we're still here, something went wrong - shouldn't reach here
    while(1) {
        __asm__ __volatile__("hlt");
    }
}

void shell_execute_command() {
    // Ignore empty commands
    if (cmd_buffer[0] == '\0') {
        return;
    }
    
    // Parse and execute command
    if (strcmp(cmd_buffer, "help") == 0) {
        cmd_help();
    }
    else if (strcmp(cmd_buffer, "clear") == 0) {
        cmd_clear();
    }
    else if (strcmp(cmd_buffer, "meminfo") == 0) {
        cmd_meminfo();
    }
    else if (strcmp(cmd_buffer, "memtest") == 0) {
        cmd_memtest();
    }
    else if (strcmp(cmd_buffer, "lsblk") == 0) {
        cmd_lsblk();
    }
    else if (strcmp(cmd_buffer, "diskinfo") == 0) {
        cmd_diskinfo();
    }
    else if (starts_with(cmd_buffer, "diskread")) {
        cmd_diskread();
    }
    else if (starts_with(cmd_buffer, "diskwrite")) {
        cmd_diskwrite();
    }
    else if (strcmp(cmd_buffer, "ls") == 0) {
        fat32_list_root();
    }
    else if (starts_with(cmd_buffer, "cat ")) {
        char *args = cmd_buffer + 4;
        while (*args == ' ') args++;
        if (*args) {
            fat32_cat_file(args);
        } else {
            kprint("Usage: cat <filename>\n");
        }
    }
    else if (strcmp(cmd_buffer, "free") == 0) {
        cmd_free();
    }
    else if (starts_with(cmd_buffer, "echo")) {
        cmd_echo();
    }
    else if (strcmp(cmd_buffer, "lsacpi") == 0) {
        acpi_lsacpi();
    }
    else if (strcmp(cmd_buffer, "lscpu") == 0) {
        cmd_lscpu();
    }
    else if (strcmp(cmd_buffer, "reboot") == 0) {
        cmd_reboot();
    }
    else {
        kprint("Unknown command: ");
        kprint(cmd_buffer);
        putc('\n');
    }
}
