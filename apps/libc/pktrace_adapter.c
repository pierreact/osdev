// Libc-side pktrace output adapter. Wraps the stdio.h primitives so
// the shared pktrace.c (built from src/net/pktrace.c) can emit traces
// uniformly in both kernel and app contexts.

#include <net/pktrace.h>
#include "stdio.h"

void pktrace_put_str(const char *s) {
    puts(s);
}

void pktrace_put_dec(uint64 n) {
    print_dec(n);
}

void pktrace_put_hex(uint64 n) {
    for (int i = 7; i >= 0; i--)
        print_hex8((uint8)((n >> (i * 8)) & 0xFF));
}

void pktrace_put_char(char c) {
    putc(c);
}
