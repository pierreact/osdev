#ifndef SYSTEM_CPU_H
#define SYSTEM_CPU_H

#include <types.h>

#define MAX_CPUS 16
#define AP_STACK_SIZE 16384  // 16KB stack per AP

// Per-CPU state, indexed by CPU number (0 = BSP, 1+ = APs).
// Padded to 16 bytes so the trampoline can index by LAPIC ID with a shift.
typedef struct {
    uint8   lapic_id;
    uint8   running;       // Set to 1 by AP after it finishes startup
    uint8   reserved[6];
    uint64  stack_top;     // Points to top of this CPU's stack (grows down)
} PerCPU;

extern PerCPU percpu[MAX_CPUS];

void cpu_init(void);

#endif
