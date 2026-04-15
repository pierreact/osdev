#ifndef ISURUS_NET_L2_H
#define ISURUS_NET_L2_H

#include <types.h>
#include <net/eth.h>
#include <net/arp.h>
#include <net/pktrace.h>

// Backend vtable -- abstracts nic_send/nic_recv for dual kernel/userland compilation
typedef struct {
    int  (*send)(void *ctx, const uint8 *frame, uint32 len);
    int  (*recv)(void *ctx, uint8 *buf, uint32 *len);
    void (*get_mac)(void *ctx, uint8 *mac_out);
} NetBackend;

// Always-on counters (all traffic, not just traced)
typedef struct {
    uint64 rx_frames;
    uint64 tx_frames;
    uint64 rx_bytes;
    uint64 tx_bytes;
    uint64 arp_requests_sent;
    uint64 arp_replies_sent;
} L2Stats;

// Per-NIC L2 context. One per managed interface.
typedef struct {
    NetBackend  backend;
    void       *backend_ctx;        // nic_index (kernel) or device ptr (DPDK)
    uint8       mac[ETH_ADDR_LEN];
    uint8       reserved[2];
    uint32      ip;                 // our IPv4 (network byte order), 0 = none
    ArpTable    arp;
    L2Stats     stats;
    uint8       frame_buf[ETH_FRAME_MAX + 2];   // scratch RX buffer (+2 alignment)
} L2Context;

// Initialize L2 context. Reads MAC from backend.
void l2_init(L2Context *ctx, NetBackend backend, void *backend_ctx, uint32 ip);

// Poll: receive one frame, dispatch by ethertype.
// ARP frames handled internally (updates table, sends replies).
// Returns 0 and fills ethertype/payload/payload_len for non-ARP frames.
// Returns -1 if no frame or frame consumed internally (ARP).
// trace may be NULL for untraced polling.
int l2_poll(L2Context *ctx, uint16 *ethertype, uint8 **payload,
            uint32 *payload_len, PkTrace *trace);

// Send a payload with Ethernet framing
int l2_send(L2Context *ctx, const uint8 *dst_mac, uint16 ethertype,
            const uint8 *payload, uint32 payload_len, PkTrace *trace);

// ARP-resolved send: look up dst IP in ARP table, send if resolved.
// If not resolved, sends ARP request and returns -1 (caller retries later).
int l2_send_ip(L2Context *ctx, uint32 dst_ip, uint16 ethertype,
               const uint8 *payload, uint32 payload_len, PkTrace *trace);

// Get stats snapshot
void l2_get_stats(L2Context *ctx, L2Stats *out);

#endif
