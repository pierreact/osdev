// Shell core: input buffer, tab completion, command dispatch table,
// and ring-3 task entry point. All command handlers and helpers live in
// sibling shell_*.c files; see shell_internal.h for the shared contract.

#include <shell/shell.h>
#include <syscall.h>
#include "shell_internal.h"

// ============================================================================
// Shared state (referenced via shell_internal.h by every shell_*.c)
// ============================================================================

char  cmd_buffer[CMD_BUFFER_SIZE];
uint8 cmd_index = 0;

// 1 = ring 3 (dispatch through syscalls), 0 = ring 0 direct kernel calls.
// Set by shell_ring3_entry before the first sh_* wrapper is called.
uint8 use_syscalls = 0;

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
    "sys.net.arp",
    "sys.net.arping",
    "sys.net.stats",
    "sys.net.trace",
    "sys.proc.run",
    "sys.proc.ls",
};
#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))

// ============================================================================
// Shell core: welcome, prompt, input handling, tab completion
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
// Command dispatch table. Prefix-matched commands have prefix=1.
// ============================================================================
typedef struct { const char *name; void (*handler)(void); uint8 prefix; } CmdEntry;

static const CmdEntry cmd_table[] = {
    {"help",           cmd_help,          0},
    {"clear",          cmd_clear,         0},
    {"echo",           cmd_echo,          1},
    {"sys.reboot",     cmd_reboot_wrap,   0},
    {"sys.cpu.ls",     cmd_lscpu,         0},
    {"sys.cpu.ring",   cmd_ring,          0},
    {"sys.mem.info",   cmd_meminfo,       0},
    {"sys.mem.free",   cmd_free,          0},
    {"sys.mem.test",   cmd_memtest,       0},
    {"sys.acpi.ls",    sh_acpi_ls,        0},
    {"sys.disk.ls",    cmd_lsblk,         0},
    {"sys.disk.info",  cmd_diskinfo,      0},
    {"sys.disk.read",  cmd_diskread,      1},
    {"sys.disk.write", cmd_diskwrite,     1},
    {"sys.fs.ls",      cmd_fs_ls,         1},
    {"sys.fs.cat",     cmd_fs_cat,        1},
    {"sys.pci.ls",     cmd_lspci,         0},
    {"sys.nic.ls",     cmd_lsnic,         0},
    {"sys.nic.mode",   cmd_nic_mode,      1},
    {"sys.thread.ls",  cmd_thread_ls,     0},
    {"sys.test.ap",    cmd_test_ap_wrap,  0},
    {"sys.net.arp",    cmd_net_arp,       0},
    {"sys.net.arping", cmd_net_arping,    1},
    {"sys.net.stats",  cmd_net_stats,     0},
    {"sys.net.trace",  cmd_net_trace,     0},
    {"sys.proc.run",   cmd_proc_run,      1},
    {"sys.proc.ls",    cmd_proc_ls,       0},
    {0, 0, 0}
};

void shell_execute_command() {
    while (cmd_index > 0 && cmd_buffer[cmd_index - 1] == ' ') {
        cmd_index--;
        cmd_buffer[cmd_index] = '\0';
    }
    if (cmd_buffer[0] == '\0') return;

    for (const CmdEntry *e = cmd_table; e->name; e++) {
        if (e->prefix) {
            if (starts_with(cmd_buffer, e->name)) { e->handler(); return; }
        } else {
            if (strcmp(cmd_buffer, e->name) == 0) { e->handler(); return; }
        }
    }

    sh_print("Unknown command: ");
    sh_print(cmd_buffer);
    sh_putc('\n');
}

// Ring 3 entry point - sets use_syscalls flag and runs the shell loop
void shell_ring3_entry(void) {
    use_syscalls = 1;
    shell_init();
    while (1) {
        int c = sys_wait_input();
        if (c != 0)
            shell_handle_char((char)c);
    }
}
