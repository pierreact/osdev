#ifndef KERNEL_LOADER_H
#define KERNEL_LOADER_H

#include <types.h>
#include <kernel/cpu.h>

#define USER_LOAD_ADDR    0x2000000   // 32MB, above page tables (~9MB), must match apps/link.ld
#define USER_STACK_SIZE   0x10000     // 64KB per AP user stack

// Per-AP state (set by loader or app launcher before dispatch)
extern uint64 ap_user_stacks[MAX_CPUS];
extern uint64 ap_entry_addrs[MAX_CPUS];

// AP ring 3 launcher (called on AP via ap_dispatch, does not return)
uint64 ap_run_ring3(uint64 cpu_idx);

// Load a flat binary from ISO and execute it on all APs in ring 3.
// Each AP runs the binary's _start in ring 3, syscalls back when done.
// Returns 0 on success (all APs completed).
int loader_exec(const char *iso_path);

#endif
