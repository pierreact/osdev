// ICMP Echo handling, shared between kernel and libc builds.

#include <net/icmp.h>
#include <net/ip.h>
#include <net/eth.h>
#include <memops.h>

int icmp_send_echo(L2Context *ctx, uint32 dst_ip, uint16 id, uint16 seq,
                   const uint8 *payload, uint32 payload_len,
                   PkTrace *trace) {
    uint16 mtu = ctx->mtu ? ctx->mtu : ETH_MTU;
    if (payload_len + ICMP_ECHO_HDR_LEN + IPV4_HDR_LEN > mtu)
        return -1;

    uint8 msg[ETH_MTU];
    IcmpEchoHdr *eh = (IcmpEchoHdr *)msg;
    eh->type     = ICMP_TYPE_ECHO_REQUEST;
    eh->code     = 0;
    eh->checksum = 0;
    eh->id       = htons(id);
    eh->seq      = htons(seq);
    if (payload_len > 0)
        memcpy(msg + ICMP_ECHO_HDR_LEN, payload, payload_len);

    uint32 msg_len = ICMP_ECHO_HDR_LEN + payload_len;
    eh->checksum = ip_checksum(msg, msg_len);

    int rc = ip_send(ctx, dst_ip, IP_PROTO_ICMP, msg, msg_len, trace);
    if (rc == 0)
        ctx->ip_stats.icmp_echo_tx++;
    return rc;
}

int icmp_rx(L2Context *ctx, uint32 src_ip, uint8 *msg, uint32 msg_len,
            PkTrace *trace) {
    if (msg_len < ICMP_ECHO_HDR_LEN) {
        ctx->ip_stats.ipv4_dropped++;
        return -1;
    }

    IcmpEchoHdr *eh = (IcmpEchoHdr *)msg;

    // Verify ICMP checksum (covers the full ICMP message).
    uint16 got = eh->checksum;
    eh->checksum = 0;
    uint16 calc = ip_checksum(msg, msg_len);
    eh->checksum = got;
    if (calc != got) {
        ctx->ip_stats.ipv4_dropped++;
        return -1;
    }

    if (eh->type == ICMP_TYPE_ECHO_REQUEST) {
        PKT_STAMP(trace, PKT_ICMP_ECHO_RX, 0, 0, msg_len);
        ctx->ip_stats.icmp_echo_rx++;

        // Build the reply: same id/seq/payload, type=0, new checksum.
        uint8 reply[ETH_MTU];
        uint32 reply_len = msg_len;
        memcpy(reply, msg, msg_len);
        IcmpEchoHdr *rh = (IcmpEchoHdr *)reply;
        rh->type     = ICMP_TYPE_ECHO_REPLY;
        rh->code     = 0;
        rh->checksum = 0;
        rh->checksum = ip_checksum(reply, reply_len);

        int rc = ip_send(ctx, src_ip, IP_PROTO_ICMP, reply, reply_len, trace);
        if (rc == 0) {
            PKT_STAMP(trace, PKT_ICMP_ECHO_TX, 0, 0, reply_len);
            ctx->ip_stats.icmp_echo_tx++;
        }
        return rc;
    }

    if (eh->type == ICMP_TYPE_ECHO_REPLY) {
        // Count the reply; higher-level ping command inspects stats
        // and/or pktrace to detect responses in this milestone.
        ctx->ip_stats.icmp_echo_rx++;
        PKT_STAMP(trace, PKT_ICMP_ECHO_RX, 0, 0, msg_len);
        (void)src_ip;
        return 0;
    }

    // Unsupported ICMP type.
    ctx->ip_stats.ipv4_dropped++;
    return -1;
}
