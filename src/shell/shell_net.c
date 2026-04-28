// Network-related shell commands: NIC enumeration, NIC mode, per-thread
// NIC binding, ARP table, ARPing, L2 stats, and packet trace.

#include "shell_internal.h"
#include <arch/ports.h>
#include <drivers/pci.h>
#include <kernel/cpu.h>
#include <arch/acpi.h>
#include <net/nic.h>
#include <net/l2.h>
#include <net/l2_kern.h>
#include <net/arp.h>
#include <net/ip.h>
#include <net/icmp.h>
#include <net/pktbuf.h>
#include <net/pktrace.h>
#include <net/eth.h>
#include <services/net_service.h>

void cmd_lsnic(void) {
    uint32 count = nic_get_count();
    if (count == 0) {
        sh_print("No NICs found\n");
        return;
    }
    for (uint32 i = 0; i < count; i++) {
        sh_print((char *)nic_name(i));
        sh_print("  NUMA ");
        uint32 node = nic_get_numa_node(i);
        if (node == PCI_NUMA_UNKNOWN)
            sh_putc('-');
        else
            sh_print_dec(node);
        sh_print("  MAC ");
        uint8 mac[6];
        nic_get_mac(i, mac);
        for (int j = 0; j < 6; j++) {
            sh_print_hex8(mac[j]);
            if (j < 5) sh_putc(':');
        }
        sh_print(nic_link_status(i) ? "  link up" : "  link down");
        sh_putc('\n');
    }
}

void cmd_nic_mode(void) {
    char *args = cmd_buffer + strlen("sys.nic.mode");
    while (*args == ' ') args++;
    if (*args == '\0') {
        sh_print("mode: ");
        print_mode(nic_get_mode());
        sh_putc('\n');
        return;
    }
    if (strcmp(args, "per-numa") == 0) {
        nic_set_mode(NIC_MODE_PER_NUMA);
        nic_assign();
        sh_print("mode: per-numa\n");
    } else if (strcmp(args, "per-core") == 0) {
        nic_set_mode(NIC_MODE_PER_CORE);
        nic_assign();
        sh_print("mode: per-core\n");
    } else {
        sh_print("Usage: sys.nic.mode [per-numa|per-core]\n");
    }
}

void cmd_thread_ls(void) {
    sh_print("mode: ");
    print_mode(nic_get_mode());
    sh_putc('\n');
    sh_print("CPU  NUMA  NIC          PCI         MAC\n");
    for (uint32 i = 0; i < cpu_count; i++) {
        ThreadMeta *tm = thread_meta_get(i);
        sh_print_dec_pad(i, 3);
        sh_print("  ");
        if (tm->numa_node == THREAD_NUMA_UNKNOWN) {
            sh_print("  -");
        } else {
            sh_print_dec_pad(tm->numa_node, 3);
        }
        sh_print("  ");
        if (tm->nic_index == NIC_NONE) {
            sh_print("(none)       ");
            sh_print("           ");
            sh_print("                 ");
        } else {
            const char *name = nic_name(tm->nic_index);
            sh_print(name ? (char *)name : "?");
            // pad name to 13 chars
            int nlen = 0;
            if (name) { while (name[nlen] && nlen < 13) nlen++; }
            for (int p = nlen; p < 13; p++) sh_putc(' ');
            sh_print_hex8(tm->nic_bus);
            sh_putc(':');
            sh_print_hex8(tm->nic_dev);
            sh_putc('.');
            sh_print_dec(tm->nic_func);
            sh_print("    ");
            for (int j = 0; j < 6; j++) {
                sh_print_hex8(tm->nic_mac[j]);
                if (j < 5) sh_putc(':');
            }
        }
        sh_putc('\n');
    }
}

void cmd_net_arp(void) {
    NetContext *ctx = l2_kern_mgmt();
    net_service_drain(ctx);
    sh_print("ARP table (mgmt NIC 0):\n");
    for (uint32 i = 0; i < ARP_TABLE_SIZE; i++) {
        ArpEntry *e = &ctx->arp.entries[i];
        if (!e->valid) continue;
        sh_print("  ");
        print_ipv4(e->ip);
        sh_print("  ");
        for (int j = 0; j < 6; j++) {
            sh_print_hex8(e->mac[j]);
            if (j < 5) sh_putc(':');
        }
        sh_putc('\n');
    }
}

void cmd_net_arping(void) {
    // Parse IP from command: "sys.net.arping 10.0.2.2"
    const char *args = cmd_buffer + 15;  // skip "sys.net.arping "
    while (*args == ' ') args++;
    if (*args == '\0') {
        sh_print("Usage: sys.net.arping <ip>\n");
        return;
    }

    uint32 ip = htonl(parse_ipv4(args));
    if (ip == 0) {
        sh_print("Invalid IP\n");
        return;
    }

    NetContext *ctx = l2_kern_mgmt();
    sh_print("ARPING ");
    print_ipv4(ip);
    sh_putc('\n');

    uint64 tsc_start = rdtsc();
    arp_request(ctx, ip);

    // Poll for reply (timeout ~1 second at ~2GHz)
    uint64 timeout = 2000000000ULL;
    while (rdtsc() - tsc_start < timeout) {
        uint16 etype;
        uint8 *payload;
        uint32 plen;
        l2_poll(ctx, &etype, &payload, &plen, NULL);

        const uint8 *mac = arp_lookup(&ctx->arp, ip);
        if (mac) {
            uint64 elapsed = rdtsc() - tsc_start;
            sh_print("  Reply: ");
            for (int j = 0; j < 6; j++) {
                sh_print_hex8(mac[j]);
                if (j < 5) sh_putc(':');
            }
            sh_print("  ");
            sh_print_dec(elapsed);
            sh_print(" cycles\n");
            return;
        }
    }

    sh_print("  Timeout\n");
}

void cmd_net_stats(void) {
    NetContext *ctx = l2_kern_mgmt();
    net_service_drain(ctx);
    L2Stats st;
    l2_get_stats(ctx, &st);
    sh_print("L2 stats (mgmt NIC 0):\n");
    sh_print("  rx_frames: ");      sh_print_dec(st.rx_frames);      sh_putc('\n');
    sh_print("  tx_frames: ");      sh_print_dec(st.tx_frames);      sh_putc('\n');
    sh_print("  rx_bytes:  ");      sh_print_dec(st.rx_bytes);       sh_putc('\n');
    sh_print("  tx_bytes:  ");      sh_print_dec(st.tx_bytes);       sh_putc('\n');
    sh_print("  rx_drop:   ");      sh_print_dec(st.rx_dropped);        sh_putc('\n');
    sh_print("  rx_arp:    ");      sh_print_dec(st.rx_arp);            sh_putc('\n');
    sh_print("  arp_req:   ");      sh_print_dec(st.arp_requests_sent); sh_putc('\n');
    sh_print("  arp_reply: ");      sh_print_dec(st.arp_replies_sent);  sh_putc('\n');
    sh_print("  pool:      ");
    sh_print_dec(pktbuf_pool_used(&ctx->pool));
    sh_putc('/');
    sh_print_dec(pktbuf_pool_total(&ctx->pool));
    sh_print(" bufs\n");

    IpStats ips;
    ip_get_stats(ctx, &ips);
    sh_print("IP stats:\n");
    sh_print("  ipv4_rx:       "); sh_print_dec(ips.ipv4_rx);              sh_putc('\n');
    sh_print("  ipv4_tx:       "); sh_print_dec(ips.ipv4_tx);              sh_putc('\n');
    sh_print("  ipv4_dropped:  "); sh_print_dec(ips.ipv4_dropped);         sh_putc('\n');
    sh_print("  ipv4_oversize: "); sh_print_dec(ips.ipv4_dropped_oversize);sh_putc('\n');
    sh_print("  ttl_expired:   "); sh_print_dec(ips.ttl_expired);          sh_putc('\n');
    sh_print("  forwarded:     "); sh_print_dec(ips.forwarded);            sh_putc('\n');
    sh_print("  icmp_echo_rx:  "); sh_print_dec(ips.icmp_echo_rx);         sh_putc('\n');
    sh_print("  icmp_echo_tx:  "); sh_print_dec(ips.icmp_echo_tx);         sh_putc('\n');

    NetServiceStats ns;
    net_service_get_stats(&ns);
    sh_print("net_service stats:\n");
    sh_print("  ticks:         "); sh_print_dec(ns.ticks);                  sh_putc('\n');
    sh_print("  frames:        "); sh_print_dec(ns.frames_processed);       sh_putc('\n');
    sh_print("  max/tick:      "); sh_print_dec(ns.frames_per_tick_max);    sh_putc('\n');
    sh_print("  idle ticks:    "); sh_print_dec(ns.ticks_with_zero_frames); sh_putc('\n');
    sh_print("  isr_count:     "); sh_print_dec(ns.isr_count);              sh_putc('\n');
}

void cmd_net_ip(void) {
    NetContext *ctx = l2_kern_mgmt();
    sh_print("mgmt NIC 0:\n");
    sh_print("  ip:   "); print_ipv4(ctx->ip);   sh_putc('\n');
    sh_print("  mask: "); print_ipv4(ctx->mask); sh_putc('\n');
    sh_print("  gw:   "); print_ipv4(ctx->gw);   sh_putc('\n');
    sh_print("  mtu:  "); sh_print_dec(ctx->mtu ? ctx->mtu : ETH_MTU); sh_putc('\n');
    sh_print("  fwd:  "); sh_print_dec(ctx->forward); sh_putc('\n');
}

void cmd_net_route(void) {
    NetContext *ctx = l2_kern_mgmt();
    sh_print("Routes (mgmt NIC 0):\n");
    if (ctx->mask != 0) {
        sh_print("  "); print_ipv4(ctx->ip & ctx->mask);
        sh_print("/");  print_ipv4(ctx->mask);
        sh_print("  on-link\n");
    }
    if (ctx->gw != 0) {
        sh_print("  0.0.0.0/0        via "); print_ipv4(ctx->gw); sh_putc('\n');
    }
}

void cmd_net_ping(void) {
    // "sys.net.ping 10.0.2.2"
    const char *args = cmd_buffer + 13;   // skip "sys.net.ping "
    while (*args == ' ') args++;
    if (*args == '\0') {
        sh_print("Usage: sys.net.ping <ip>\n");
        return;
    }

    uint32 ip = htonl(parse_ipv4(args));
    if (ip == 0) {
        sh_print("Invalid IP\n");
        return;
    }

    NetContext *ctx = l2_kern_mgmt();
    sh_print("PING ");
    print_ipv4(ip);
    sh_putc('\n');

    // Prime ARP if needed so the first request doesn't just
    // bounce off a missing MAC. ip_send triggers an arp_request
    // internally on miss; we loop briefly to let it resolve.
    uint32 next_hop = ip;
    if (ctx->mask != 0 && (ip & ctx->mask) != (ctx->ip & ctx->mask))
        next_hop = ctx->gw;

    uint64 arp_deadline = rdtsc() + 1000000000ULL;
    while (!arp_lookup(&ctx->arp, next_hop) && rdtsc() < arp_deadline) {
        arp_request(ctx, next_hop);
        ctx->stats.arp_requests_sent++;
        // drain some replies
        for (int i = 0; i < 32; i++) {
            uint16 et; uint8 *p; uint32 pl;
            if (l2_poll(ctx, &et, &p, &pl, NULL) == L2_EMPTY) break;
        }
    }

    IpStats before;
    ip_get_stats(ctx, &before);

    uint64 tsc_start = rdtsc();
    int rc = icmp_send_echo(ctx, ip, 0x1234, 1, NULL, 0, NULL);
    if (rc != 0) {
        sh_print("  Send failed (ARP pending?)\n");
        return;
    }

    uint64 timeout = 2000000000ULL;
    while (rdtsc() - tsc_start < timeout) {
        uint16 et; uint8 *p; uint32 pl;
        int r = l2_poll(ctx, &et, &p, &pl, NULL);
        if (r == L2_EMPTY) continue;
        if (r == L2_OK && et == ETH_TYPE_IPV4)
            ip_rx(ctx, p, pl, NULL);

        IpStats now;
        ip_get_stats(ctx, &now);
        if (now.icmp_echo_rx > before.icmp_echo_rx) {
            uint64 elapsed = rdtsc() - tsc_start;
            sh_print("  Reply from ");
            print_ipv4(ip);
            sh_print("  ");
            sh_print_dec(elapsed);
            sh_print(" cycles\n");
            return;
        }
    }

    sh_print("  Timeout\n");
}

void cmd_net_trace(void) {
    // Read trace log directly (kernel memory is readable from ring 3)
    // but print via sh_print (syscalls) since kprint GPFs from ring 3.
    uint32 count, head;
    PkTrace *log = pktrace_get_log(&count, &head);
    if (count == 0) {
        sh_print("TRACE: no records\n");
        return;
    }
    sh_print("TRACE: ");
    sh_print_dec(count);
    sh_print(" record(s)\n");

    uint32 start = 0;
    if (head > count) start = head - count;

    for (uint32 i = start; i < head; i++) {
        PkTrace *t = &log[i % PKTRACE_LOG_SIZE];
        if (t->stamp_count == 0) continue;

        sh_print_hex(t->tag, "");
        sh_print(" s");
        sh_print_dec(t->seq);
        sh_putc('\n');

        uint64 prev_tsc = 0;
        for (uint8 j = 0; j < t->stamp_count; j++) {
            PkTraceStamp *s = &t->stamps[j];
            uint64 delta = (prev_tsc > 0) ? (s->tsc - prev_tsc) : 0;
            sh_print("  ");
            sh_print((char *)pktrace_point_name(s->point));
            sh_print("  +");
            sh_print_dec(delta);
            sh_print(" cyc  buf ");
            sh_print_dec(s->buf_used);
            sh_putc('/');
            sh_print_dec(s->buf_capacity);
            sh_print("  len ");
            sh_print_dec(s->payload_len);
            sh_putc('\n');
            prev_tsc = s->tsc;
        }
    }
}
