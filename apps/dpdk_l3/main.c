// Isurus DPDK L3 demo.
//
// One pinned ring-3 thread per AP core. Each polls its local NIC,
// runs L2+L3 through the shared libc stack, and answers ICMP Echo
// Requests addressed to its manifest-supplied IP. Non-IP traffic is
// ignored. Bounded by MAX_ITERATIONS like dpdk_l2 until the kernel
// dispatcher goes non-blocking.

#include "../libc/isurus.h"
#include "../libc/stdio.h"
#include "../libc/string.h"
#include "../libc/nic_backend.h"
#include <net/l2.h>
#include <net/eth.h>
#include <net/ip.h>
#include <net/icmp.h>
#include <net/pktrace.h>

#define POOL_PAGES     32
#define PRINT_MASK     ((1ULL << 19) - 1)
#define MAX_ITERATIONS (1ULL << 20)

static uint8 pool_memory[POOL_PAGES * 4096];

static void print_ip(uint32 ip) {
    uint8 *b = (uint8 *)&ip;
    print_dec(b[0]); putc('.');
    print_dec(b[1]); putc('.');
    print_dec(b[2]); putc('.');
    print_dec(b[3]);
}

__attribute__((section(".text.start")))
void _start(ThreadMeta *meta) {
    if (meta->cpu_index == 0) exit();

    if (meta->nic_index == NIC_NONE) {
        puts("[dpdk_l3] cpu="); print_dec(meta->cpu_index);
        puts(" no local NIC, exit\n");
        exit();
    }

    AppNetCfg cfg;
    if (app_net_cfg(&cfg) != 0 || cfg.ip == 0) {
        puts("[dpdk_l3] cpu="); print_dec(meta->cpu_index);
        puts(" no IP config, exit\n");
        exit();
    }

    L2Context ctx;
    NetBackend be = nic_backend_make(meta);
    memset(pool_memory, 0, sizeof(pool_memory));
    l2_init(&ctx, be, (void *)meta, cfg.ip, POOL_PAGES, pool_memory);
    ctx.mask    = cfg.mask;
    ctx.gw      = cfg.gw;
    ctx.mtu     = cfg.mtu ? cfg.mtu : 1500;
    ctx.forward = cfg.forward;

    puts("[dpdk_l3] cpu="); print_dec(meta->cpu_index);
    puts(" nic=");          print_dec(meta->nic_index);
    puts(" ip=");           print_ip(cfg.ip);
    puts(" ready\n");

    uint64 iter = 0;
    while (iter < MAX_ITERATIONS) {
        uint16 etype;
        uint8 *payload;
        uint32 plen;
        int rc = l2_poll(&ctx, &etype, &payload, &plen, 0);
        if (rc == L2_OK) {
            switch (etype) {
                case ETH_TYPE_IPV4:
                    ip_rx(&ctx, payload, plen, 0);
                    break;
                default:
                    // Non-IP frames are ignored in the L3 demo.
                    break;
            }
        }
        iter++;
        if ((iter & PRINT_MASK) == 0) {
            IpStats ips;
            ip_get_stats(&ctx, &ips);
            puts("[dpdk_l3] cpu="); print_dec(meta->cpu_index);
            puts(" ipv4_rx="); print_dec(ips.ipv4_rx);
            puts(" icmp_rx="); print_dec(ips.icmp_echo_rx);
            puts(" icmp_tx="); print_dec(ips.icmp_echo_tx);
            putc('\n');
        }
    }

    IpStats ips;
    ip_get_stats(&ctx, &ips);
    puts("[dpdk_l3] cpu="); print_dec(meta->cpu_index);
    puts(" final ipv4_rx="); print_dec(ips.ipv4_rx);
    puts(" icmp_rx="); print_dec(ips.icmp_echo_rx);
    puts(" icmp_tx="); print_dec(ips.icmp_echo_tx);
    puts(" done\n");
    exit();
}
