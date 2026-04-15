#include <net/arp.h>
#include <net/l2.h>
#include <kernel/mem.h>

void arp_table_init(ArpTable *t) {
    memset(t, 0, sizeof(ArpTable));
}

const uint8 *arp_lookup(ArpTable *t, uint32 ip) {
    for (uint32 i = 0; i < ARP_TABLE_SIZE; i++) {
        if (t->entries[i].valid && t->entries[i].ip == ip)
            return t->entries[i].mac;
    }
    return 0;
}

void arp_insert(ArpTable *t, uint32 ip, const uint8 *mac) {
    // Update existing entry
    for (uint32 i = 0; i < ARP_TABLE_SIZE; i++) {
        if (t->entries[i].valid && t->entries[i].ip == ip) {
            memcpy(t->entries[i].mac, mac, ETH_ADDR_LEN);
            return;
        }
    }

    // Find empty slot
    for (uint32 i = 0; i < ARP_TABLE_SIZE; i++) {
        if (!t->entries[i].valid) {
            t->entries[i].ip = ip;
            memcpy(t->entries[i].mac, mac, ETH_ADDR_LEN);
            t->entries[i].valid = 1;
            t->count++;
            return;
        }
    }

    // Table full: overwrite slot 0 (simple eviction)
    t->entries[0].ip = ip;
    memcpy(t->entries[0].mac, mac, ETH_ADDR_LEN);
    t->entries[0].valid = 1;
}

int arp_process(void *l2ctx, ArpTable *t, uint8 *frame, uint32 frame_len) {
    L2Context *ctx = (L2Context *)l2ctx;

    if (frame_len < ETH_HDR_LEN + ARP_PKT_LEN)
        return -1;

    ArpPacket *arp = (ArpPacket *)(frame + ETH_HDR_LEN);

    // Only handle Ethernet/IPv4 ARP
    if (ntohs(arp->hw_type) != ARP_HW_ETHER)
        return -1;
    if (ntohs(arp->proto_type) != ETH_TYPE_IPV4)
        return -1;
    if (arp->hw_len != ETH_ADDR_LEN || arp->proto_len != 4)
        return -1;

    // Always learn sender (gratuitous learning)
    arp_insert(t, arp->sender_ip, arp->sender_mac);

    uint16 op = ntohs(arp->opcode);

    if (op == ARP_OP_REQUEST && ctx->ip != 0 && arp->target_ip == ctx->ip) {
        // ARP request for our IP -- send reply
        uint8 reply[ETH_HDR_LEN + ARP_PKT_LEN];
        eth_build_hdr(reply, arp->sender_mac, ctx->mac, ETH_TYPE_ARP);

        ArpPacket *rarp = (ArpPacket *)(reply + ETH_HDR_LEN);
        rarp->hw_type = htons(ARP_HW_ETHER);
        rarp->proto_type = htons(ETH_TYPE_IPV4);
        rarp->hw_len = ETH_ADDR_LEN;
        rarp->proto_len = 4;
        rarp->opcode = htons(ARP_OP_REPLY);
        memcpy(rarp->sender_mac, ctx->mac, ETH_ADDR_LEN);
        rarp->sender_ip = ctx->ip;
        memcpy(rarp->target_mac, arp->sender_mac, ETH_ADDR_LEN);
        rarp->target_ip = arp->sender_ip;

        ctx->backend.send(ctx->backend_ctx, reply, sizeof(reply));
        ctx->stats.arp_replies_sent++;
    }

    // ARP_OP_REPLY: already learned above via arp_insert
    return 0;
}

int arp_request(void *l2ctx, uint32 target_ip) {
    L2Context *ctx = (L2Context *)l2ctx;
    if (ctx->ip == 0)
        return -1;   // no IP configured, cannot ARP

    uint8 pkt[ETH_HDR_LEN + ARP_PKT_LEN];
    eth_build_hdr(pkt, ETH_BROADCAST, ctx->mac, ETH_TYPE_ARP);

    ArpPacket *arp = (ArpPacket *)(pkt + ETH_HDR_LEN);
    arp->hw_type = htons(ARP_HW_ETHER);
    arp->proto_type = htons(ETH_TYPE_IPV4);
    arp->hw_len = ETH_ADDR_LEN;
    arp->proto_len = 4;
    arp->opcode = htons(ARP_OP_REQUEST);
    memcpy(arp->sender_mac, ctx->mac, ETH_ADDR_LEN);
    arp->sender_ip = ctx->ip;
    memset(arp->target_mac, 0, ETH_ADDR_LEN);
    arp->target_ip = target_ip;

    int rc = ctx->backend.send(ctx->backend_ctx, pkt, sizeof(pkt));
    if (rc == 0)
        ctx->stats.arp_requests_sent++;
    return rc;
}
