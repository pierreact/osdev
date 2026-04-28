#ifndef ISURUS_NIC_BACKEND_H
#define ISURUS_NIC_BACKEND_H

// NetBackend wired to the per-core local NIC via SYS_NIC_SEND and
// SYS_NIC_RECV. The kernel resolves the NIC from the calling CPU's
// thread_meta[] entry, so a thread must never drive the same backend
// from a different core.
//
// Reusable glue for all polled userland network apps (dpdk_l2 today,
// future dpdk_l3 / dpdk_l4).

#include <types.h>
#include "isurus.h"
#include <net/l2.h>

// Build a NetBackend that drives the per-core NIC. The returned
// backend captures meta by pointer; meta must outlive the NetContext
// (the kernel-provided ThreadMeta pointer passed to _start is valid
// for the lifetime of the thread).
NetBackend nic_backend_make(const ThreadMeta *meta);

#endif
