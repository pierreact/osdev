#ifndef ISURUS_STDIO_H
#define ISURUS_STDIO_H

#include "types.h"

void putc(char c);
void puts(const char *s);
void print_dec(uint64 n);
void print_hex8(uint8 val);
void print_hex16(uint16 val);

#endif
