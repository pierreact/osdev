#ifndef ISURUS_SYSCALL_H
#define ISURUS_SYSCALL_H

#include <types.h>

// Syscall numbers (must match kernel's include/syscall.h)
#define SYS_PUTC          0
#define SYS_KPRINT        1
#define SYS_CLS           2
#define SYS_KPRINT_DEC    16
#define SYS_KPRINT_HEX    17
#define SYS_YIELD         21
#define SYS_TASK_EXIT     22
#define SYS_NIC_SEND      27
#define SYS_NIC_RECV      28

// Raw syscall interface (x86-64 SYSCALL convention)
// SYSCALL clobbers: RCX (saved RIP), R11 (saved RFLAGS).
// The kernel's syscall_entry also clobbers RDI, RSI, RDX, R8, R9, R10
// when remapping registers for the C calling convention, and does not
// restore them before SYSRET. All must be declared as clobbers (except
// those already used as input constraints).
static inline long syscall0(long nr) {
    long ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(nr)
        : "rcx", "r11", "rdi", "rsi", "rdx", "r8", "r9", "r10", "memory");
    return ret;
}

static inline long syscall1(long nr, long a1) {
    long ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(nr), "D"(a1)
        : "rcx", "r11", "rsi", "rdx", "r8", "r9", "r10", "memory");
    return ret;
}

static inline long syscall2(long nr, long a1, long a2) {
    long ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(nr), "D"(a1), "S"(a2)
        : "rcx", "r11", "rdx", "r8", "r9", "r10", "memory");
    return ret;
}

#endif
