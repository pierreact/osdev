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

// AP work dispatch: BSP sets ready=1, AP polls and calls fn(arg),
// sets result and done=1.
typedef struct {
    volatile uint8  ready;      // offset 0: BSP sets to 1 to dispatch
    volatile uint8  done;       // offset 1: AP sets to 1 when finished
    uint8           reserved[6];// offset 2-7
    uint64          fn;         // offset 8: function pointer
    uint64          arg;        // offset 16: argument
    uint64          result;     // offset 24: return value from fn
} APWork;

#define APWORK_SIZE       32
#define APWORK_OFF_READY   0
#define APWORK_OFF_DONE    1
#define APWORK_OFF_FN      8
#define APWORK_OFF_ARG    16
#define APWORK_OFF_RESULT 24

extern APWork ap_work[MAX_CPUS];

// Dispatch a function to an AP. Blocks until the AP completes.
// Returns the function's return value.
uint64 ap_dispatch(uint32 cpu_idx, uint64 (*fn)(uint64 arg), uint64 arg);

// Dispatch to all APs, wait for all to complete.
void ap_dispatch_all(uint64 (*fn)(uint64 arg), uint64 arg);

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
