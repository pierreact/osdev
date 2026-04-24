#ifndef ISURUS_H
#define ISURUS_H

#include <types.h>
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

// Per-app L3 config as parsed from the INI manifest by the kernel.
// Zero means "unset". ip/mask/gw are network byte order.
typedef struct {
    uint32 ip;
    uint32 mask;
    uint32 gw;
    uint16 mtu;
    uint8  forward;
    uint8  reserved;
} AppNetCfg;

// Fetch the calling core's manifest-parsed L3 config. Returns 0 on
// success, -1 if the caller is not an app-dispatched AP core.
static inline int app_net_cfg(AppNetCfg *out) {
    return (int)syscall1(SYS_APP_NET_CFG, (long)out);
}

#endif
