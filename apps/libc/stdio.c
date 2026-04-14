#include "stdio.h"
#include "syscall.h"

void putc(char c) {
    syscall1(SYS_PUTC, c);
}

void puts(const char *s) {
    syscall1(SYS_KPRINT, (long)s);
}

void print_dec(uint64 n) {
    syscall1(SYS_KPRINT_DEC, (long)n);
}

static const char hex_chars[] = "0123456789abcdef";

void print_hex8(uint8 val) {
    putc(hex_chars[(val >> 4) & 0xF]);
    putc(hex_chars[val & 0xF]);
}

void print_hex16(uint16 val) {
    print_hex8((val >> 8) & 0xFF);
    print_hex8(val & 0xFF);
}
