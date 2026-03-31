#ifndef SYSTEM_CPU_H
#define SYSTEM_CPU_H

#include <types.h>

#define MAX_CPUS 16
#define AP_STACK_SIZE 16384  // 16KB stack per AP

// Per-CPU state, indexed by CPU number (0 = BSP, 1+ = APs).
// Size must match PERCPU_SIZE in ap_trampoline.asm.
typedef struct {
    uint8   lapic_id;
    uint8   running;       // Set to 1 by AP after it finishes startup
    uint8   in_usermode;   // 1 when CPU is executing ring 3 code
    uint8   reserved[5];
    uint64  stack_top;     // Kernel stack top (ring 0, grows down)
    uint64  user_stack_top;// User stack top (ring 3, grows down)
    uint64  user_rsp;      // Saved user RSP during syscall/interrupt
    uint64  cr3;           // Page table root for this CPU
} PerCPU;

#define PERCPU_SIZE 40
// Offsets for assembly access (must match struct layout)
#define PERCPU_OFF_STACK_TOP    8
#define PERCPU_OFF_USER_RSP     24
#define PERCPU_OFF_CR3          32

extern PerCPU percpu[MAX_CPUS];

void cpu_init(void);

#endif
