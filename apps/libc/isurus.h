#ifndef ISURUS_H
#define ISURUS_H

#include "types.h"
#include "syscall.h"

// Thread metadata: per-CPU snapshot exposed by the kernel.
// Must match kernel/cpu.h ThreadMeta layout exactly.
typedef struct {
    uint32 cpu_index;
    uint32 numa_node;
    uint32 nic_index;
    uint16 nic_segment;
    uint8  nic_bus;
    uint8  nic_dev;
    uint8  nic_func;
    uint8  reserved[3];
    uint8  nic_mac[6];
    uint8  reserved2[2];
} ThreadMeta;

#define NIC_NONE 0xFFFFFFFFu
#define THREAD_NUMA_UNKNOWN 0xFFFFFFFFu

// Exit the current thread (AP re-parks, BSP task exits)
static inline void exit(void) {
    syscall0(SYS_TASK_EXIT);
    // unreachable
    for (;;) {}
}

// Yield to next BSP task (no-op on APs)
static inline void yield(void) {
    syscall0(SYS_YIELD);
}

#endif
