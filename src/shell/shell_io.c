// Output abstraction layer + formatting helpers.
// Every sh_* wrapper dispatches to either a direct kernel call (ring 0,
// during boot) or a syscall (ring 3, after task_init) based on use_syscalls.

#include "shell_internal.h"
#include <drivers/monitor.h>
#include <kernel/mem.h>
#include <drivers/ide.h>
#include <fs/fat32.h>
#include <arch/acpi.h>
#include <net/nic.h>
#include <net/eth.h>
#include <syscall.h>

void sh_putc(char c) {
    if (use_syscalls) sys_putc(c);
    else putc(c);
}

void sh_print(char *s) {
    if (use_syscalls) sys_kprint(s);
    else kprint(s);
}

void sh_cls(void) {
    if (use_syscalls) sys_cls();
    else cls();
}

void sh_print_dec(uint64 n) {
    if (use_syscalls) sys_kprint_dec(n);
    else kprint_dec(n);
}

void sh_print_hex(long n, char *post) {
    if (use_syscalls) sys_kprint_hex(n, post);
    else kprint_long2hex(n, post);
}

void sh_print_dec_pad(uint64 n, uint32 w) {
    if (use_syscalls) sys_kprint_dec_pad(n, w);
    else kprint_dec_pad(n, w);
}

void *sh_malloc(size_t sz) {
    if (use_syscalls) return sys_kmalloc(sz);
    else return kmalloc(sz);
}

void sh_free(void *p) {
    if (use_syscalls) sys_kfree(p);
    else kfree(p);
}

void sh_heap_stats(uint32 *u, uint32 *f, uint32 *t) {
    if (use_syscalls) sys_heap_stats(u, f, t);
    else heap_stats(u, f, t);
}

int sh_ide_read(uint32 lba, uint8 cnt, uint8 *buf) {
    if (use_syscalls) return sys_ide_read(lba, cnt, buf);
    else return ide_read_sectors(lba, cnt, buf);
}

int sh_ide_write(uint32 lba, uint8 *buf) {
    if (use_syscalls) return sys_ide_write(lba, buf);
    else return ide_write_sector(lba, buf);
}

char *sh_ide_model(void) {
    if (use_syscalls) return sys_ide_model();
    else return ide_get_model();
}

uint32 sh_ide_sectors(void) {
    if (use_syscalls) return sys_ide_sectors();
    else return ide_get_sector_count();
}

void sh_fat32_ls(void) {
    if (use_syscalls) sys_fat32_ls();
    else fat32_list_root();
}

int sh_fat32_cat(const char *n) {
    if (use_syscalls) return sys_fat32_cat(n);
    else return fat32_cat_file(n);
}

void sh_acpi_ls(void) {
    if (use_syscalls) sys_acpi_ls();
    else acpi_lsacpi();
}

// ============================================================================
// Formatting helpers
// ============================================================================

static const char hex_tbl[] = "0123456789abcdef";

void sh_print_hex16(uint16 val) {
    sh_putc(hex_tbl[(val >> 12) & 0xF]);
    sh_putc(hex_tbl[(val >> 8) & 0xF]);
    sh_putc(hex_tbl[(val >> 4) & 0xF]);
    sh_putc(hex_tbl[val & 0xF]);
}

void sh_print_hex8(uint8 val) {
    sh_putc(hex_tbl[(val >> 4) & 0xF]);
    sh_putc(hex_tbl[val & 0xF]);
}

void hex_dump(uint8 *data, uint32 len) {
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

void print_ipv4(uint32 ip_net) {
    uint32 ip = ntohl(ip_net);
    sh_print_dec((ip >> 24) & 0xFF); sh_putc('.');
    sh_print_dec((ip >> 16) & 0xFF); sh_putc('.');
    sh_print_dec((ip >> 8) & 0xFF);  sh_putc('.');
    sh_print_dec(ip & 0xFF);
}

void print_mode(NicAssignmentMode m) {
    sh_print(m == NIC_MODE_PER_CORE ? "per-core" : "per-numa");
}

// Redraw the current input line in place. Uses only \b and spaces so
// it works uniformly over serial, telnet, and the VGA monitor
// (src/drivers/monitor.c: putc handles both and mirrors to COM1).
//
// Step 1: walk the cursor left back to just-after-prompt (old_cursor
//         backspaces).
// Step 2: print the new buffer (new_len chars).
// Step 3: if the new buffer is shorter, erase the stale trailing chars
//         by printing spaces, then backing up over them.
// Step 4: if the cursor should end up mid-line, back up the right
//         number of positions from end-of-buffer.
void sh_redraw_line(const char *buf, uint8 new_len, uint8 new_cursor,
                    uint8 old_len, uint8 old_cursor) {
    for (uint8 i = 0; i < old_cursor; i++) sh_putc('\b');
    for (uint8 i = 0; i < new_len; i++)    sh_putc(buf[i]);

    if (new_len < old_len) {
        uint8 diff = (uint8)(old_len - new_len);
        for (uint8 i = 0; i < diff; i++) sh_putc(' ');
        for (uint8 i = 0; i < diff; i++) sh_putc('\b');
    }

    if (new_cursor < new_len) {
        uint8 back = (uint8)(new_len - new_cursor);
        for (uint8 i = 0; i < back; i++) sh_putc('\b');
    }
}
