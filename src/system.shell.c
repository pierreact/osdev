#include <system.shell.h>
#include <system.monitor.h>
#include <system.ports.h>
#include <system.mem.h>

// Command buffer
#define CMD_BUFFER_SIZE 80
static char cmd_buffer[CMD_BUFFER_SIZE];
static uint8 cmd_index = 0;

// External symbols for memory info
extern uint8 data_counter_mmap_entries;
extern uint32 MEMMAP_START;
extern uint32 PML4T_LOCATION;
extern uint32 PAGING_LOCATION_END;

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
    // Note: cls() removed to keep boot messages visible
    kprint("\n");  // Add spacing after boot messages
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
    kprint("  help     - Show this help message\n");
    kprint("  clear    - Clear the screen\n");
    kprint("  meminfo  - Display memory information\n");
    kprint("  echo     - Echo text to screen\n");
    kprint("  reboot   - Reboot the system\n");
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

void cmd_echo() {
    // Skip "echo" and any spaces
    char *text = cmd_buffer + 4;  // Skip "echo"
    while (*text == ' ') text++;
    
    if (*text) {
        kprint(text);
        putc('\n');
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
    else if (starts_with(cmd_buffer, "echo")) {
        cmd_echo();
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
