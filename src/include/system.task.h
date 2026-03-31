#ifndef SYSTEM_TASK_H
#define SYSTEM_TASK_H

#include <types.h>

#define MAX_BSP_TASKS 8
#define TASK_STACK_PAGES 16             // 64KB per task stack

typedef enum {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_WAITING,
} TaskState;

// Saved register frame pushed on kernel stack during syscall/yield.
// Must match the push order in asm64_syscall.inc and task_switch assembly.
typedef struct {
    uint64 r15;
    uint64 r14;
    uint64 r13;
    uint64 r12;
    uint64 rbp;
    uint64 rbx;
    uint64 r11;         // user RFLAGS (from SYSCALL)
    uint64 rcx;         // user RIP (from SYSCALL)
} TaskRegs;

typedef struct {
    uint64    kernel_rsp;   // saved kernel RSP (points to TaskRegs on kernel stack)
    uint64    user_rsp;     // saved user RSP
    uint64    entry;        // ring 3 entry point (used for first launch only)
    uint64    stack_base;   // bottom of allocated user stack
    uint64    kstack_base;  // bottom of allocated kernel stack
    uint64    kstack_top;   // top of kernel stack (for TSS RSP0)
    uint64    cr3;          // page table root
    TaskState state;
    uint8     id;
    char      name[16];
} Task;

void task_init(void);
int  task_create(const char *name, void (*entry)(void));
void task_yield(void);
void task_exit(void);
void task_wake(uint8 task_id);
void task_run_first(void);

// Current task ID (for syscall handlers to check)
extern uint8 current_task_id;

#endif
