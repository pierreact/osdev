#ifndef ISURUS_SYSCALL_H
#define ISURUS_SYSCALL_H

#include "types.h"

// Syscall numbers (must match kernel's include/syscall.h)
#define SYS_PUTC          0
#define SYS_KPRINT        1
#define SYS_CLS           2
#define SYS_KPRINT_DEC    16
#define SYS_KPRINT_HEX    17
#define SYS_YIELD         21
#define SYS_TASK_EXIT     22

// Raw syscall interface (x86-64 SYSCALL convention)
static inline long syscall0(long nr) {
    long ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(nr) : "rcx", "r11", "memory");
    return ret;
}

static inline long syscall1(long nr, long a1) {
    long ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(nr), "D"(a1) : "rcx", "r11", "memory");
    return ret;
}

static inline long syscall2(long nr, long a1, long a2) {
    long ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(nr), "D"(a1), "S"(a2) : "rcx", "r11", "memory");
    return ret;
}

#endif
