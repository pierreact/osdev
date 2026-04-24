// IPv4 receive + send, shared between kernel and libc builds.

#include <net/ip.h>
#include <net/icmp.h>
#include <net/eth.h>
#include <net/arp.h>
#include <memops.h>

// One's-complement checksum (RFC 1071). Accepts odd lengths by
// treating the final byte as the high half of a 16-bit word.
uint16 ip_checksum(const void *buf, uint32 len) {
    const uint8 *p = (const uint8 *)buf;
    uint32 sum = 0;
    while (len >= 2) {
        sum += ((uint32)p[0] << 8) | p[1];
        p += 2;
        len -= 2;
    }
    if (len == 1)
        sum += (uint32)p[0] << 8;
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    uint16 csum = (uint16)~sum;
    // Return in network byte order so callers can store directly.
    return htons(csum);
}

int ip_build_hdr(uint8 *buf, uint32 src_ip, uint32 dst_ip,
                 uint8 proto, uint16 payload_len) {
    Ipv4Hdr *h = (Ipv4Hdr *)buf;
    h->ver_ihl   = (IPV4_VERSION << 4) | IPV4_IHL_MIN;
    h->tos       = 0;
    h->total_len = htons((uint16)(IPV4_HDR_LEN + payload_len));
    h->id        = 0;
    h->flags_frag = htons(IPV4_FLAG_DF);
    h->ttl       = IPV4_DEFAULT_TTL;
    h->proto     = proto;
    h->checksum  = 0;
    h->src_ip    = src_ip;
    h->dst_ip    = dst_ip;
    h->checksum  = ip_checksum(buf, IPV4_HDR_LEN);
    return IPV4_HDR_LEN;
}

int ip_send(L2Context *ctx, uint32 dst_ip, uint8 proto,
            const uint8 *payload, uint32 payload_len, PkTrace *trace) {
    uint16 mtu = ctx->mtu ? ctx->mtu : ETH_MTU;
    if (payload_len + IPV4_HDR_LEN > mtu) {
        ctx->ip_stats.ipv4_dropped_oversize++;
        return -1;
    }

    uint8 datagram[ETH_MTU];
    ip_build_hdr(datagram, ctx->ip, dst_ip, proto, (uint16)payload_len);
    if (payload_len > 0)
        memcpy(datagram + IPV4_HDR_LEN, payload, payload_len);

    // Pick next hop: on-link if dst is in our subnet, else gateway.
    uint32 next_hop = dst_ip;
    if (ctx->mask != 0 && (dst_ip & ctx->mask) != (ctx->ip & ctx->mask))
        next_hop = ctx->gw;

    const uint8 *mac = arp_lookup(&ctx->arp, next_hop);
    if (!mac) {
        arp_request(ctx, next_hop);
        ctx->stats.arp_requests_sent++;
        return -1;
    }

    int rc = l2_send(ctx, mac, ETH_TYPE_IPV4, datagram,
                     IPV4_HDR_LEN + payload_len, trace);
    if (rc == 0)
        ctx->ip_stats.ipv4_tx++;
    return rc;
}

int ip_rx(L2Context *ctx, uint8 *payload, uint32 payload_len, PkTrace *trace) {
    if (payload_len < IPV4_HDR_LEN) {
        ctx->ip_stats.ipv4_dropped++;
        return -1;
    }

    Ipv4Hdr *h = (Ipv4Hdr *)payload;
    uint8 version = h->ver_ihl >> 4;
    uint8 ihl     = h->ver_ihl & 0x0F;
    if (version != IPV4_VERSION || ihl != IPV4_IHL_MIN) {
        // Reject non-v4 and options (IHL > 5). No reassembly either.
        ctx->ip_stats.ipv4_dropped++;
        return -1;
    }

    uint16 total_len = ntohs(h->total_len);
    if (total_len > payload_len || total_len < IPV4_HDR_LEN) {
        ctx->ip_stats.ipv4_dropped++;
        return -1;
    }

    // Drop any fragment (offset non-zero or MF set). No reassembly.
    uint16 ff = ntohs(h->flags_frag);
    if ((ff & IPV4_FRAG_OFF_MASK) != 0 || (ff & IPV4_FLAG_MF) != 0) {
        ctx->ip_stats.ipv4_dropped++;
        return -1;
    }

    // Verify checksum. ip_checksum returns network-byte-order csum.
    uint16 got = h->checksum;
    h->checksum = 0;
    uint16 calc = ip_checksum(payload, IPV4_HDR_LEN);
    h->checksum = got;
    if (calc != got) {
        ctx->ip_stats.ipv4_dropped++;
        return -1;
    }

    uint32 plen = total_len - IPV4_HDR_LEN;

    PKT_STAMP(trace, PKT_IP_PARSE, 0, 0, plen);
    ctx->ip_stats.ipv4_rx++;

    // Destination = us?
    if (h->dst_ip == ctx->ip) {
        PKT_STAMP(trace, PKT_IP_DELIVER, 0, 0, plen);
        if (h->proto == IP_PROTO_ICMP) {
            return icmp_rx(ctx, h->src_ip, payload + IPV4_HDR_LEN, plen, trace);
        }
        // Unknown / unsupported protocol for us.
        ctx->ip_stats.ipv4_dropped++;
        return -1;
    }

    // Not for us. Forward if configured and TTL allows.
    if (!ctx->forward) {
        ctx->ip_stats.ipv4_dropped++;
        return -1;
    }

    if (h->ttl <= 1) {
        ctx->ip_stats.ttl_expired++;
        return -1;
    }
    h->ttl--;

    // Recompute checksum after TTL change. Simple full re-csum;
    // incremental could be added later.
    h->checksum = 0;
    h->checksum = ip_checksum(payload, IPV4_HDR_LEN);

    uint32 next_hop = h->dst_ip;
    if (ctx->mask != 0 && (h->dst_ip & ctx->mask) != (ctx->ip & ctx->mask))
        next_hop = ctx->gw;

    const uint8 *mac = arp_lookup(&ctx->arp, next_hop);
    if (!mac) {
        arp_request(ctx, next_hop);
        ctx->stats.arp_requests_sent++;
        ctx->ip_stats.ipv4_dropped++;
        return -1;
    }

    PKT_STAMP(trace, PKT_IP_FORWARD, 0, 0, plen);
    int rc = l2_send(ctx, mac, ETH_TYPE_IPV4, payload, total_len, trace);
    if (rc == 0)
        ctx->ip_stats.forwarded++;
    return rc;
}

void ip_get_stats(L2Context *ctx, IpStats *out) {
    memcpy(out, &ctx->ip_stats, sizeof(IpStats));
}
