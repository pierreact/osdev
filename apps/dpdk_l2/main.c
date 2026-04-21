// Isurus DPDK L2 reflector.
//
// One pinned ring-3 thread per AP core. Each polls its assigned local
// NIC (same CPU, same NUMA node per nic_assign), MAC-swaps every
// non-ARP frame addressed to our MAC or broadcast, and retransmits
// via SYS_NIC_SEND. ARP is handled inside libc's l2_poll.
//
// AP-only: BSP is untouched. No interrupts, no kernel network logic
// in the data path - only the thin SYS_NIC_SEND/SYS_NIC_RECV passthrough.

#include "../libc/isurus.h"
#include "../libc/stdio.h"
#include "../libc/string.h"
#include "../libc/nic_backend.h"
#include "../libc/net/l2.h"
#include "../libc/net/eth.h"
#include "../libc/net/pktrace.h"

#define POOL_PAGES     32                 // 128 KB / core (32 * 4KB)
#define PRINT_MASK     ((1ULL << 19) - 1) // stats cadence: every 2^19 polls
// Bounded run per core: kernel app_launch dispatches cores serially
// and blocks until each returns (src/kernel/app.c, marked TODO there).
// Once the dispatcher goes non-blocking, remove MAX_ITERATIONS so each
// pinned AP polls forever. Each poll hits SYS_NIC_RECV (~1 us in
// QEMU), so 1 M iters ~ 1 s wall time per core.
#define MAX_ITERATIONS (1ULL << 20)

static uint8 pool_memory[POOL_PAGES * 4096];

// Forward declaration so _start can be first in source order. Flat
// binary entry is offset 0 of .text; gcc emits functions in source
// order, so _start must come first.
static void print_stats(uint32 cpu, const L2Context *ctx,
                        uint64 cyc_min, uint64 cyc_max,
                        uint64 cyc_sum, uint64 samples);

__attribute__((section(".text.start")))
void _start(ThreadMeta *meta) {
    // AP-only safeguard. The kernel already refuses core 0 for
    // app_launch (src/kernel/app.c), this is defense in depth.
    if (meta->cpu_index == 0) exit();

    if (meta->nic_index == NIC_NONE) {
        puts("[dpdk_l2] cpu="); print_dec(meta->cpu_index);
        puts(" no local NIC, exit\n");
        exit();
    }

    L2Context ctx;
    NetBackend be = nic_backend_make(meta);
    memset(pool_memory, 0, sizeof(pool_memory));
    l2_init(&ctx, be, (void *)meta, 0 /* no IP at L2 */,
            POOL_PAGES, pool_memory);

    puts("[dpdk_l2] cpu="); print_dec(meta->cpu_index);
    puts(" nic=");          print_dec(meta->nic_index);
    puts(" ready\n");

    uint64 iter = 0;
    uint64 samples = 0, cyc_sum = 0;
    uint64 cyc_min = ~0ULL, cyc_max = 0;

    while (iter < MAX_ITERATIONS) {
        uint16 etype;
        uint8 *payload;
        uint32 plen;
        uint64 t0 = rdtsc();
        int rc = l2_poll(&ctx, &etype, &payload, &plen, 0);
        if (rc == L2_OK) {
            // Reflector: swap src/dst MAC. The libc l2_poll has
            // already validated the Ethernet header and confirmed
            // the frame is addressed to our MAC (unicast) or
            // broadcast. Pull the original source MAC out of
            // ctx.frame_buf and bounce the payload back at it.
            // Note: for broadcast input, the reply goes unicast to
            // the original sender, so no storm.
            EthHdr *h = (EthHdr *)ctx.frame_buf;
            uint8 orig_src[6];
            memcpy(orig_src, h->src, 6);
            // L2/L3/L4 dispatch hook. Today only the default arm is
            // used (reflect). Future dpdk_l3 / dpdk_l4 can branch on
            // ETH_TYPE_IPV4 / ETH_TYPE_IPV6 here.
            switch (etype) {
                default:
                    l2_send(&ctx, orig_src, etype, payload, plen, 0);
                    break;
            }
            uint64 dt = rdtsc() - t0;
            if (dt < cyc_min) cyc_min = dt;
            if (dt > cyc_max) cyc_max = dt;
            cyc_sum += dt;
            samples++;
        }
        if ((iter++ & PRINT_MASK) == 0 && iter > 1) {
            print_stats(meta->cpu_index, &ctx,
                        cyc_min, cyc_max, cyc_sum, samples);
            // Reset latency window so each stats line covers the
            // last interval only.
            cyc_min = ~0ULL; cyc_max = 0; cyc_sum = 0; samples = 0;
        }
    }
    print_stats(meta->cpu_index, &ctx,
                cyc_min, cyc_max, cyc_sum, samples);
    puts("[dpdk_l2] cpu="); print_dec(meta->cpu_index); puts(" done\n");
    exit();
}

static void print_stats(uint32 cpu, const L2Context *ctx,
                        uint64 cyc_min, uint64 cyc_max,
                        uint64 cyc_sum, uint64 samples) {
    L2Stats st;
    l2_get_stats((L2Context *)ctx, &st);
    puts("[dpdk_l2] cpu="); print_dec(cpu);
    puts(" rx=");  print_dec(st.rx_frames);
    puts(" tx=");  print_dec(st.tx_frames);
    puts(" rxB="); print_dec(st.rx_bytes);
    puts(" txB="); print_dec(st.tx_bytes);
    if (samples > 0) {
        puts(" rtt_cyc ");
        print_dec(cyc_min); putc('/');
        print_dec(cyc_sum / samples); putc('/');
        print_dec(cyc_max);
    }
    putc('\n');
}
