#ifndef ISURUS_SERVICES_NET_SERVICE_H
#define ISURUS_SERVICES_NET_SERVICE_H

// BSP cooperative net_service: continuously drains the BSP-owned
// L2Contexts so passive responders (ARP reply, ICMP echo, future
// telnet, future Prometheus exporter) work without the user having
// to type a sys.net.* shell command between every packet.
//
// Hooks into the sys_wait_input hlt loop alongside
// app_check_completion. See src/services/README.md for the contract,
// invariants, and the call chain.

#include <types.h>
#include <net/l2.h>

// Kernel-wide stats. One instance, lives in net_service.c.
// Per-NIC counts live in L2Stats / IpStats / UdpStats.
typedef struct {
    uint64 ticks;                   // net_service_tick() called this many times
    uint64 frames_processed;        // total frames drained across all contexts
    uint64 frames_per_tick_max;     // largest single-tick drain (regression sentinel)
    uint64 ticks_with_zero_frames;  // ticks where every context returned L2_EMPTY
} NetServiceStats;

// One-time setup. Zeroes stats. Safe to call before any L2Context
// is initialized: the registered list is checked for NULL each tick.
void net_service_init(void);

// Bounded drain of every BSP-owned L2Context. Called from the
// sys_wait_input hlt loop on every wake. Hard-capped at 16 frames
// per context per tick to bound DoS / starvation. Non-reentrant.
void net_service_tick(void);

// Single-context drain helper. Public replacement for what used to
// be the static drain_mgmt_nic in shell_net.c. Used by sys.net.*
// shell handlers before they print stats so the displayed counters
// reflect any frames that arrived between commands. Bounded the
// same way as the tick.
void net_service_drain(L2Context *ctx);

// Snapshot the kernel-wide stats.
void net_service_get_stats(NetServiceStats *out);

// Future: L4 port -> handler dispatch table. Lands with the first
// real consumer (telnet, the first UDP service, or the Prometheus
// exporter). Documented in src/services/README.md so the eventual
// shape is recorded; not implemented today to avoid premature
// abstraction.

#endif
