#ifndef SYSTEM_SHELL_INTERNAL_H
#define SYSTEM_SHELL_INTERNAL_H

// Internal contract between the shell_*.c translation units.
// Not installed under include/ - callers outside src/shell/ must use
// <shell/shell.h> for the public API.

#include <types.h>
#include <net/nic.h>

// ============================================================================
// Shared state (defined in shell.c)
// ============================================================================
#define CMD_BUFFER_SIZE 80
extern char  cmd_buffer[CMD_BUFFER_SIZE];
extern uint8 cmd_len;        // bytes currently in cmd_buffer
extern uint8 cmd_cursor;     // cursor position in [0, cmd_len]
extern uint8 use_syscalls;   // 1 = ring 3 (syscall wrappers), 0 = ring 0

// ============================================================================
// Output abstraction + formatting helpers (shell_io.c)
// ============================================================================
void  sh_putc(char c);
void  sh_print(char *s);
void  sh_cls(void);
void  sh_print_dec(uint64 n);
void  sh_print_hex(long n, char *post);
void  sh_print_dec_pad(uint64 n, uint32 w);
void *sh_malloc(size_t sz);
void  sh_free(void *p);
void  sh_heap_stats(uint32 *u, uint32 *f, uint32 *t);
int   sh_ide_read(uint32 lba, uint8 cnt, uint8 *buf);
int   sh_ide_write(uint32 lba, uint8 *buf);
char *sh_ide_model(void);
uint32 sh_ide_sectors(void);
void  sh_fat32_ls(void);
int   sh_fat32_cat(const char *n);
void  sh_acpi_ls(void);

void sh_print_hex8(uint8 val);
void sh_print_hex16(uint16 val);
void hex_dump(uint8 *data, uint32 len);
void print_ipv4(uint32 ip_net);
void print_mode(NicAssignmentMode m);

// Redraw the current input line in-place. Walks the cursor back to
// just-after-prompt, prints the new buffer, erases any stale trailing
// chars, then moves the cursor to its new position. Uses only \b and
// spaces so it works on serial, telnet, and the VGA monitor.
void sh_redraw_line(const char *buf, uint8 new_len, uint8 new_cursor,
                    uint8 old_len, uint8 old_cursor);

// ============================================================================
// String / parse utilities (shell_util.c)
// ============================================================================
int    strcmp(const char *s1, const char *s2);
int    strncmp(const char *s1, const char *s2, size_t n);
size_t strlen(const char *s);
int    starts_with(const char *str, const char *prefix);
uint32 parse_number(const char *str);
uint32 parse_ipv4(const char *s);

// ============================================================================
// Command handlers (one prototype per cmd_*, referenced by cmd_table[] in shell.c)
// ============================================================================

// shell_sys.c
void cmd_help(void);
void cmd_clear(void);
void cmd_echo(void);
void cmd_ring(void);
void cmd_reboot(void);
void cmd_reboot_wrap(void);
void cmd_test_ap_wrap(void);
void cmd_lspci(void);
void cmd_proc_run(void);
void cmd_proc_ls(void);

// shell_mem.c
void cmd_meminfo(void);
void cmd_memtest(void);
void cmd_free(void);

// shell_cpu.c
void cmd_lscpu(void);

// shell_disk.c
void cmd_lsblk(void);
void cmd_diskinfo(void);
void cmd_diskread(void);
void cmd_diskwrite(void);

// shell_fs.c
void cmd_fs_ls(void);
void cmd_fs_cat(void);

// shell_net.c
void cmd_lsnic(void);
void cmd_nic_mode(void);
void cmd_thread_ls(void);
void cmd_net_arp(void);
void cmd_net_arping(void);
void cmd_net_stats(void);
void cmd_net_trace(void);

#endif // SYSTEM_SHELL_INTERNAL_H
