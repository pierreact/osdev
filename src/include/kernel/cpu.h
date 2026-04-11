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

// Thread metadata: per-CPU snapshot of "what does this core/thread see?"
// Filled at boot by nic_assign(). When ring 3 AP threads land, this
// struct will be mapped read-only into each thread's address space at
// a fixed virtual address so threads can read it without syscalls.
typedef struct {
    uint32 cpu_index;       // CPU number (0..MAX_CPUS-1)
    uint32 numa_node;       // NUMA proximity of this core (or 0xFFFFFFFF)
    uint32 nic_index;       // NIC slot index, or NIC_NONE
    uint16 nic_segment;     // PCI segment of assigned NIC
    uint8  nic_bus;         // PCI bus
    uint8  nic_dev;         // PCI device
    uint8  nic_func;        // PCI function
    uint8  reserved[3];
    uint8  nic_mac[6];      // MAC address copied from NIC
    uint8  reserved2[2];
} ThreadMeta;

#define THREAD_NUMA_UNKNOWN 0xFFFFFFFFu

extern ThreadMeta thread_meta[MAX_CPUS];

void cpu_init(void);
ThreadMeta *thread_meta_get(uint32 cpu_idx);

#endif
