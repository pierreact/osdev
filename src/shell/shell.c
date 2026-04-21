// Shell core: line editor (cursor, insert, backspace), ANSI CSI
// parser for arrow keys, 4 KB command-history ring, tab completion,
// command dispatch, and ring-3 task entry point. Per-command handlers
// live in sibling shell_*.c files; see shell_internal.h for the
// cross-file contract.

#include <shell/shell.h>
#include <syscall.h>
#include "shell_internal.h"

// ============================================================================
// Shared state (referenced via shell_internal.h by every shell_*.c)
// ============================================================================

char  cmd_buffer[CMD_BUFFER_SIZE];
uint8 cmd_len    = 0;   // bytes currently in cmd_buffer
uint8 cmd_cursor = 0;   // cursor position in [0, cmd_len]

// 1 = ring 3 (dispatch through syscalls), 0 = ring 0 direct kernel calls.
// Set by shell_ring3_entry before the first sh_* wrapper is called.
uint8 use_syscalls = 0;

// ============================================================================
// ANSI CSI parser state (per-byte feed from shell_handle_char)
// ============================================================================
// 0 = normal, 1 = saw ESC (0x1B), 2 = saw CSI introducer '['.
// ESC sequences not matching ESC '[' <final-byte> are silently dropped.
static uint8 esc_state = 0;

// ============================================================================
// Command history ring: 4096 bytes of [len:u8][bytes...] entries.
// Oldest at hist_head, one-past-newest at hist_tail (both mod size).
// hist_used = bytes currently stored, hist_count = number of entries.
// On push, consecutive duplicates are skipped; overflow evicts oldest
// whole entries until the new one fits.
// ============================================================================
#define HIST_BUF_SIZE 4096
static uint8  hist_buf[HIST_BUF_SIZE];
static uint32 hist_head = 0;
static uint32 hist_tail = 0;
static uint32 hist_used = 0;
static uint32 hist_count = 0;

// Navigation state. hist_nav == -1 means we are not in recall mode.
// Valid indices are [0, hist_count-1], 0 = oldest, hist_count-1 = newest.
// hist_saved_* holds the in-progress line before the user started
// cycling, so Down past the newest entry restores it.
static int32 hist_nav = -1;
static char  hist_saved_line[CMD_BUFFER_SIZE];
static uint8 hist_saved_len = 0;
static uint8 hist_saved_cursor = 0;

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
// Welcome + prompt
// ============================================================================

void shell_init() {
    sh_print("\n");
    sh_print("Welcome!\n");
    sh_print("Type 'help' for available commands.\n\n");
    cmd_len = 0;
    cmd_cursor = 0;
    cmd_buffer[0] = '\0';
    shell_prompt();
}

void shell_prompt() {
    sh_print("> ");
}

// ============================================================================
// History ring primitives
// ============================================================================

static uint8 hist_read_byte(uint32 off) {
    return hist_buf[off % HIST_BUF_SIZE];
}

static void hist_write_byte(uint32 off, uint8 b) {
    hist_buf[off % HIST_BUF_SIZE] = b;
}

// Evict the oldest entry. Caller must ensure hist_count > 0.
static void hist_evict_oldest(void) {
    uint8 oldlen = hist_read_byte(hist_head);
    hist_head = (hist_head + 1 + oldlen) % HIST_BUF_SIZE;
    hist_used -= (1 + oldlen);
    hist_count--;
}

// Compare the most recent history entry to buf/len. Returns 1 on match.
static int hist_last_equals(const char *buf, uint8 len) {
    if (hist_count == 0) return 0;
    // Walk from head counting count-1 entries, land on the last one.
    uint32 off = hist_head;
    for (uint32 i = 0; i < hist_count - 1; i++) {
        off = (off + 1 + hist_read_byte(off)) % HIST_BUF_SIZE;
    }
    if (hist_read_byte(off) != len) return 0;
    for (uint8 i = 0; i < len; i++) {
        if (hist_read_byte(off + 1 + i) != (uint8)buf[i]) return 0;
    }
    return 1;
}

// Push a new command. Skips if empty or duplicate of last. Evicts
// oldest entries until the new one fits.
static void history_push(const char *buf, uint8 len) {
    if (len == 0) return;
    // TEMP: dedup disabled for diag
    // if (hist_last_equals(buf, len)) return;
    uint32 need = 1 + (uint32)len;
    if (need > HIST_BUF_SIZE) return;
    while (hist_used + need > HIST_BUF_SIZE && hist_count > 0) {
        hist_evict_oldest();
    }
    hist_write_byte(hist_tail, len);
    for (uint8 i = 0; i < len; i++) {
        hist_write_byte(hist_tail + 1 + i, (uint8)buf[i]);
    }
    hist_tail = (hist_tail + need) % HIST_BUF_SIZE;
    hist_used += need;
    hist_count++;
}

// Load entry idx (0 = oldest) into out, write length to *out_len.
// Returns 1 on success, 0 if idx >= hist_count.
static int history_get(uint32 idx, char *out, uint8 *out_len) {
    if (idx >= hist_count) return 0;
    uint32 off = hist_head;
    for (uint32 i = 0; i < idx; i++) {
        off = (off + 1 + hist_read_byte(off)) % HIST_BUF_SIZE;
    }
    uint8 len = hist_read_byte(off);
    for (uint8 i = 0; i < len; i++) {
        out[i] = (char)hist_read_byte(off + 1 + i);
    }
    *out_len = len;
    return 1;
}

// ============================================================================
// Line-editing helpers (all update cmd_buffer/cmd_len/cmd_cursor and
// then redraw)
// ============================================================================

static void line_save_before_nav(void) {
    // Called before the first Up in a nav session. Captures the
    // current in-progress line so Down past the newest can restore it.
    hist_saved_len = cmd_len;
    hist_saved_cursor = cmd_cursor;
    for (uint8 i = 0; i < cmd_len; i++) hist_saved_line[i] = cmd_buffer[i];
}

static void line_replace(const char *src, uint8 new_len) {
    uint8 old_len = cmd_len;
    uint8 old_cursor = cmd_cursor;
    if (new_len > CMD_BUFFER_SIZE - 1) new_len = CMD_BUFFER_SIZE - 1;
    for (uint8 i = 0; i < new_len; i++) cmd_buffer[i] = src[i];
    cmd_buffer[new_len] = '\0';
    cmd_len = new_len;
    cmd_cursor = new_len;
    sh_redraw_line(cmd_buffer, cmd_len, cmd_cursor, old_len, old_cursor);
}

static void line_insert(char c) {
    if (cmd_len >= CMD_BUFFER_SIZE - 1) return;
    // Fast path: appending at end of line. Just echo the byte and
    // advance - no redraw, no shift. This is the hot path for
    // normal typing and avoids N^2 serial traffic.
    if (cmd_cursor == cmd_len) {
        cmd_buffer[cmd_len++] = c;
        cmd_cursor = cmd_len;
        cmd_buffer[cmd_len] = '\0';
        sh_putc(c);
        return;
    }
    // Mid-line insert: shift tail right by 1 and redraw.
    uint8 old_len = cmd_len;
    uint8 old_cursor = cmd_cursor;
    for (uint8 i = cmd_len; i > cmd_cursor; i--) {
        cmd_buffer[i] = cmd_buffer[i - 1];
    }
    cmd_buffer[cmd_cursor] = c;
    cmd_len++;
    cmd_cursor++;
    cmd_buffer[cmd_len] = '\0';
    sh_redraw_line(cmd_buffer, cmd_len, cmd_cursor, old_len, old_cursor);
}

static void line_backspace(void) {
    if (cmd_cursor == 0) return;
    // Fast path: backspace at end of line. Emit \b <space> \b - same
    // as the legacy shell, VGA erases a char, serial treats as
    // destructive BS.
    if (cmd_cursor == cmd_len) {
        cmd_len--;
        cmd_cursor--;
        cmd_buffer[cmd_len] = '\0';
        sh_putc('\b');
        sh_putc(' ');
        sh_putc('\b');
        return;
    }
    // Mid-line backspace: shift tail left by 1 and redraw.
    uint8 old_len = cmd_len;
    uint8 old_cursor = cmd_cursor;
    for (uint8 i = cmd_cursor - 1; i < cmd_len - 1; i++) {
        cmd_buffer[i] = cmd_buffer[i + 1];
    }
    cmd_len--;
    cmd_cursor--;
    cmd_buffer[cmd_len] = '\0';
    sh_redraw_line(cmd_buffer, cmd_len, cmd_cursor, old_len, old_cursor);
}

static void line_cursor_left(void) {
    if (cmd_cursor == 0) return;
    sh_putc('\b');
    cmd_cursor--;
}

static void line_cursor_right(void) {
    if (cmd_cursor >= cmd_len) return;
    sh_putc(cmd_buffer[cmd_cursor]);
    cmd_cursor++;
}

static void history_prev(void) {
    if (hist_count == 0) return;
    if (hist_nav == -1) {
        line_save_before_nav();
        hist_nav = (int32)hist_count - 1;  // start at newest
    } else if (hist_nav > 0) {
        hist_nav--;
    } else {
        return;  // already at oldest
    }
    char buf[CMD_BUFFER_SIZE];
    uint8 len = 0;
    if (history_get((uint32)hist_nav, buf, &len)) {
        line_replace(buf, len);
    }
}

static void history_next(void) {
    if (hist_nav == -1) return;
    if ((uint32)hist_nav + 1 < hist_count) {
        hist_nav++;
        char buf[CMD_BUFFER_SIZE];
        uint8 len = 0;
        if (history_get((uint32)hist_nav, buf, &len)) {
            line_replace(buf, len);
        }
    } else {
        // Past newest: restore pre-nav snapshot.
        hist_nav = -1;
        line_replace(hist_saved_line, hist_saved_len);
        // line_replace parks cursor at end; restore saved cursor.
        uint8 old_len = cmd_len;
        uint8 old_cursor = cmd_cursor;
        cmd_cursor = hist_saved_cursor;
        if (cmd_cursor < old_cursor) {
            for (uint8 i = 0; i < old_cursor - cmd_cursor; i++) sh_putc('\b');
        }
        (void)old_len;
    }
}

// ============================================================================
// Tab completion (only active when cursor is at end of line)
// ============================================================================

static void shell_tab_complete(void) {
    if (cmd_len == 0) return;
    if (cmd_cursor != cmd_len) return;   // tab-complete requires end-of-line

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
                uint32 j = cmd_len;
                while (j < common_len && first_match[j] == commands[i][j])
                    j++;
                common_len = j;
            }
        }
    }

    if (match_count == 0) return;

    if (common_len > cmd_len) {
        for (uint32 i = cmd_len; i < common_len && cmd_len < CMD_BUFFER_SIZE - 1; i++) {
            char ch = first_match[i];
            cmd_buffer[cmd_len++] = ch;
            sh_putc(ch);
        }
        cmd_cursor = cmd_len;
        cmd_buffer[cmd_len] = '\0';
        if (match_count == 1 && cmd_len < CMD_BUFFER_SIZE - 1) {
            cmd_buffer[cmd_len++] = ' ';
            cmd_cursor = cmd_len;
            cmd_buffer[cmd_len] = '\0';
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
        for (uint8 i = 0; i < cmd_len; i++)
            sh_putc(cmd_buffer[i]);
    }
}

// ============================================================================
// Input dispatch: ANSI CSI parser + editing keys
// ============================================================================

void shell_handle_char(char c) {
    if (esc_state == 1) {
        if (c == '[') { esc_state = 2; return; }
        esc_state = 0;  // unknown ESC sequence, drop
        return;
    }
    if (esc_state == 2) {
        esc_state = 0;
        switch (c) {
            case 'A': history_prev();    return;
            case 'B': history_next();    return;
            case 'C': line_cursor_right(); return;
            case 'D': line_cursor_left();  return;
            default: return;  // unhandled CSI final byte, drop
        }
    }

    // esc_state == 0: normal byte
    if (c == 0x1B) { esc_state = 1; return; }

    if (c == 0x0A) {
        sh_putc('\n');
        cmd_buffer[cmd_len] = '\0';
        history_push(cmd_buffer, cmd_len);
        hist_nav = -1;
        shell_execute_command();
        cmd_len = 0;
        cmd_cursor = 0;
        cmd_buffer[0] = '\0';
        shell_prompt();
        return;
    }
    if (c == 0x09) { shell_tab_complete(); return; }
    if (c == 0x08) { line_backspace(); return; }

    // Printable ASCII
    if ((uint8)c >= 0x20 && (uint8)c < 0x7F) {
        line_insert(c);
    }
    // Other control chars silently dropped.
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
    // Trim trailing spaces in-place.
    while (cmd_len > 0 && cmd_buffer[cmd_len - 1] == ' ') {
        cmd_len--;
        cmd_buffer[cmd_len] = '\0';
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
