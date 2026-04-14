#ifndef KERNEL_LOADER_H
#define KERNEL_LOADER_H

#include <types.h>

#define USER_LOAD_ADDR    0x2000000   // 32MB — above page tables (~9MB), must match apps/link.ld
#define USER_STACK_SIZE   0x10000     // 64KB per AP user stack

// Load a flat binary from ISO and execute it on all APs in ring 3.
// Each AP runs the binary's _start in ring 3, syscalls back when done.
// Returns 0 on success (all APs completed).
int loader_exec(const char *iso_path);

#endif
