// Userland L2 implementation (mirrors src/net/l2.c with userland includes)
#include "net/l2.h"
#include "string.h"

void l2_init(L2Context *ctx, NetBackend backend, void *backend_ctx, uint32 ip) {
    memset(ctx, 0, sizeof(L2Context));
    ctx->backend = backend;
    ctx->backend_ctx = backend_ctx;
    ctx->ip = ip;
    arp_table_init(&ctx->arp);
    backend.get_mac(backend_ctx, ctx->mac);
}

int l2_poll(L2Context *ctx, uint16 *ethertype, uint8 **payload,
            uint32 *payload_len, PkTrace *trace) {
    uint32 frame_len = 0;
    int rc = ctx->backend.recv(ctx->backend_ctx, ctx->frame_buf, &frame_len);
    if (rc < 0 || frame_len == 0) return -1;
    PKT_STAMP(trace, PKT_NIC_RX, 0, 0, frame_len);
    ctx->stats.rx_frames++;
    ctx->stats.rx_bytes += frame_len;
    EthHdr *hdr = eth_hdr(ctx->frame_buf, frame_len);
    if (!hdr) return -1;
    uint16 etype = ntohs(hdr->ethertype);
    uint32 plen = eth_payload_len(frame_len);
    PKT_STAMP(trace, PKT_L2_PARSE, 0, 0, plen);
    if (!eth_mac_eq(hdr->dst, ctx->mac) && !eth_is_broadcast(hdr->dst))
        return -1;
    if (etype == ETH_TYPE_ARP) {
        PKT_STAMP(trace, PKT_ARP_PROCESS, 0, 0, plen);
        arp_process(ctx, &ctx->arp, ctx->frame_buf, frame_len);
        return -1;
    }
    PKT_STAMP(trace, PKT_L2_DELIVER, 0, 0, plen);
    *ethertype = etype;
    *payload = eth_payload(ctx->frame_buf);
    *payload_len = plen;
    return 0;
}

int l2_send(L2Context *ctx, const uint8 *dst_mac, uint16 ethertype,
            const uint8 *payload, uint32 payload_len, PkTrace *trace) {
    if (payload_len > ETH_MTU) return -1;
    PKT_STAMP(trace, PKT_L2_SEND_ENTER, 0, 0, payload_len);
    uint8 frame[ETH_FRAME_MAX];
    eth_build_hdr(frame, dst_mac, ctx->mac, ethertype);
    memcpy(frame + ETH_HDR_LEN, payload, payload_len);
    uint32 frame_len = ETH_HDR_LEN + payload_len;
    PKT_STAMP(trace, PKT_L2_SEND_BUILT, 0, 0, frame_len);
    PKT_STAMP(trace, PKT_NIC_TX, 0, 0, frame_len);
    int rc = ctx->backend.send(ctx->backend_ctx, frame, frame_len);
    PKT_STAMP(trace, PKT_NIC_TX_DONE, 0, 0, frame_len);
    if (rc == 0) {
        ctx->stats.tx_frames++;
        ctx->stats.tx_bytes += frame_len;
    }
    return rc;
}

int l2_send_ip(L2Context *ctx, uint32 dst_ip, uint16 ethertype,
               const uint8 *payload, uint32 payload_len, PkTrace *trace) {
    const uint8 *mac = arp_lookup(&ctx->arp, dst_ip);
    if (mac) return l2_send(ctx, mac, ethertype, payload, payload_len, trace);
    arp_request(ctx, dst_ip);
    return -1;
}

void l2_get_stats(L2Context *ctx, L2Stats *out) {
    memcpy(out, &ctx->stats, sizeof(L2Stats));
}
