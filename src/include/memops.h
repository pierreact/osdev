#ifndef MEMOPS_H
#define MEMOPS_H

// Shared mem primitives for sources that compile in BOTH the kernel
// build (src/Makefile) and the libc build (apps/libc/Makefile).
// Kernel satisfies these via src/kernel/mem.c; libc satisfies them
// via apps/libc/string.c.

#include <types.h>

void *memset(void *dest, int val, size_t n);
void *memcpy(void *dest, const void *src, size_t n);

#endif
