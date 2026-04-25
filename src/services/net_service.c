// BSP cooperative net_service. Continuously drains the BSP-owned
// L2Contexts so the network stack works passively (no shell input
// required between packets). The L3 plan deferred this as a
// "kernel-side persistent L3 task"; once telnet and Prometheus join
// ICMP as consumers it stops being optional.
//
// Hooks into the sys_wait_input hlt loop. The shell task's idle is
// the foundation's heartbeat. Cooperative scheduling guarantees a
// single writer per L2Context, no locks needed.

#include <services/net_service.h>
#include <net/l2.h>
#include <net/l2_kern.h>
#include <net/eth.h>
#include <net/ip.h>

// Per-tick frame cap PER CONTEXT. Bounds DoS / starvation under host
// flood; conservative. Bump if telnet / Prometheus ever need higher
// steady-state throughput. L2_BATCH_MAX is 32 (see net/l2.h); we
// stay below that so a single noisy NIC cannot burn the whole tick.
#define NET_SERVICE_BUDGET 16

// Kernel-wide stats. One instance.
static NetServiceStats stats;

// Bumped by the virtio-net INTx ISR
// (asm64_isr_virtio_net_handler). Defined here so the symbol lives
// in .bss and the ISR has a stable address to write. volatile
// because the ISR writes asynchronously w.r.t. C reads.
volatile uint64 net_service_isr_count;

// Reentrancy guard. net_service_tick must not recurse: a shell
// handler that calls net_service_drain inside a path that itself
// reaches sys_wait_input would loop the foundation through itself.
// The shell side calls net_service_drain (not _tick), and the tick
// caller (sys_wait_input) is the only entry that flips this flag.
static uint8 in_tick;

void net_service_init(void) {
    stats.ticks = 0;
    stats.frames_processed = 0;
    stats.frames_per_tick_max = 0;
    stats.ticks_with_zero_frames = 0;
    stats.isr_count = 0;
    in_tick = 0;
    net_service_isr_count = 0;
}

// Drain one context up to NET_SERVICE_BUDGET frames. Returns the
// number of frames processed. ARP is handled inside l2_poll; IPv4
// frames go to ip_rx (which dispatches to icmp_rx and, soon, future
// L4 handlers).
static uint32 drain_one(L2Context *ctx) {
    if (!ctx) return 0;
    uint32 processed = 0;
    for (uint32 i = 0; i < NET_SERVICE_BUDGET; i++) {
        uint16 etype;
        uint8 *payload;
        uint32 plen;
        int rc = l2_poll(ctx, &etype, &payload, &plen, 0);
        if (rc == L2_EMPTY)
            break;
        if (rc == L2_OK && etype == ETH_TYPE_IPV4)
            ip_rx(ctx, payload, plen, 0);
        // L2_CONSUMED (ARP handled internally) or L2_OK non-IPv4
        // both count as a processed frame.
        processed++;
    }
    return processed;
}

void net_service_drain(L2Context *ctx) {
    drain_one(ctx);
}

void net_service_tick(void) {
    if (in_tick) return;       // never recurse
    in_tick = 1;

    stats.ticks++;

    uint32 total = 0;
    total += drain_one(l2_kern_mgmt());
    total += drain_one(l2_kern_inter());

    if (total == 0) {
        stats.ticks_with_zero_frames++;
    } else {
        stats.frames_processed += total;
        if (total > stats.frames_per_tick_max)
            stats.frames_per_tick_max = total;
    }
    // pktrace stamps live inside per-frame handlers (l2_poll, ip_rx,
    // icmp_rx). The PKT_NET_SERVICE_TICK enum entry is reserved for
    // when a real per-tick trace context exists; the current macro
    // shape needs a PkTrace * which is per-frame, not per-tick.

    in_tick = 0;
}

void net_service_get_stats(NetServiceStats *out) {
    out->ticks                  = stats.ticks;
    out->frames_processed       = stats.frames_processed;
    out->frames_per_tick_max    = stats.frames_per_tick_max;
    out->ticks_with_zero_frames = stats.ticks_with_zero_frames;
    out->isr_count              = net_service_isr_count;
}
