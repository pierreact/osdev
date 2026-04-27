#ifndef ISURUS_NET_L2_H
#define ISURUS_NET_L2_H

#include <types.h>
#include <net/eth.h>
#include <net/arp.h>
#include <net/pktrace.h>
#include <net/pktbuf.h>

// Backend vtable -- abstracts nic_send/nic_recv for dual kernel/userland compilation
typedef struct {
    int  (*send)(void *ctx, const uint8 *frame, uint32 len);
    int  (*recv)(void *ctx, uint8 *buf, uint32 *len);
    void (*get_mac)(void *ctx, uint8 *mac_out);
    // Optional batch receive: receive up to max_frames into pktbuf slots.
    // Returns number of frames received (0 if none).
    // If NULL, l2_poll_batch falls back to single recv calls.
    int  (*recv_batch)(void *ctx, PktBuf **bufs, uint32 max_frames);
} NetBackend;

// Always-on L2 counters (all traffic, not just traced)
typedef struct {
    uint64 rx_frames;
    uint64 tx_frames;
    uint64 rx_bytes;
    uint64 tx_bytes;
    uint64 rx_dropped;          // not for us (wrong MAC, not broadcast)
    uint64 rx_arp;              // frames that entered arp_process
    uint64 arp_requests_sent;
    uint64 arp_replies_sent;
} L2Stats;

// Always-on L3 counters. Lives inside NetContext because IP rides
// directly on top of L2 for this project (one IP per interface,
// per-NIC context owns both layers).
typedef struct {
    uint64 ipv4_rx;                 // IPv4 frames accepted past parse
    uint64 ipv4_tx;                 // IPv4 frames emitted
    uint64 ipv4_dropped;            // bad header / unknown proto / not for us w/o forward
    uint64 ipv4_dropped_oversize;   // would-be TX > MTU and DF set (or no-frag policy)
    uint64 ttl_expired;             // hit 0 on forward
    uint64 forwarded;               // forwarded to next hop
    uint64 icmp_echo_rx;            // echo requests received
    uint64 icmp_echo_tx;            // echo replies sent
} IpStats;

// Per-NIC L2+L3 context. One per managed interface. Carries the L3
// configuration (ip/mask/gw/mtu/forward) because L3 rides on top of
// L2 and the two contexts are tightly paired in this design.
typedef struct {
    NetBackend  backend;
    void       *backend_ctx;        // nic_index (kernel) or device ptr (DPDK)
    uint8       mac[ETH_ADDR_LEN];
    uint8       reserved[2];
    uint32      ip;                 // our IPv4 (network byte order), 0 = none
    uint32      mask;               // netmask (network byte order), 0 = /0
    uint32      gw;                 // default gateway (network byte order)
    uint16      mtu;                // 0 means "use ETH_MTU"
    uint8       forward;            // 1 = forward non-local IPv4, 0 = drop
    uint8       reserved2[5];
    ArpTable    arp;
    L2Stats     stats;
    IpStats     ip_stats;
    PktBufPool  pool;               // pre-allocated buffer pool (zero-copy IO)
    uint8       frame_buf[ETH_FRAME_MAX + 2];   // fallback RX buffer (when no pool)
} NetContext;

// Maximum frames returned by l2_poll_batch
#define L2_BATCH_MAX 32

// Initialize L2 context. Reads MAC from backend.
// pool_pages: number of 4KB pages for the buffer pool (0 = no pool, use frame_buf fallback).
void l2_init(NetContext *ctx, NetBackend backend, void *backend_ctx, uint32 ip,
             uint32 pool_pages, uint8 *pool_memory);

// Poll return codes
#define L2_OK        0   // Non-ARP frame delivered (ethertype/payload filled)
#define L2_EMPTY    -1   // No frame available from NIC
#define L2_CONSUMED -2   // Frame handled internally (ARP) or not for us

// Poll: receive one frame, dispatch by ethertype.
// ARP frames handled internally (updates table, sends replies).
// trace may be NULL for untraced polling.
int l2_poll(NetContext *ctx, uint16 *ethertype, uint8 **payload,
            uint32 *payload_len, PkTrace *trace);

// Send a payload with Ethernet framing
int l2_send(NetContext *ctx, const uint8 *dst_mac, uint16 ethertype,
            const uint8 *payload, uint32 payload_len, PkTrace *trace);

// ARP-resolved send: look up dst IP in ARP table, send if resolved.
// If not resolved, sends ARP request and returns -1 (caller retries later).
int l2_send_ip(NetContext *ctx, uint32 dst_ip, uint16 ethertype,
               const uint8 *payload, uint32 payload_len, PkTrace *trace);

// Zero-copy send: caller provides a PktBuf with payload written at pktbuf_payload().
// L2 writes the Ethernet header into the headroom in-place (no copy).
// The buffer is NOT freed -- caller manages its lifetime.
int l2_send_zc(NetContext *ctx, PktBuf *buf, const uint8 *dst_mac,
               uint16 ethertype, uint32 payload_len, PkTrace *trace);

// Batch receive: receive up to L2_BATCH_MAX frames into PktBuf slots from the pool.
// ARP frames are handled internally and not returned.
// Returns number of non-ARP frames placed in out_bufs[].
// Caller must pktbuf_free each buffer when done processing.
int l2_poll_batch(NetContext *ctx, PktBuf **out_bufs, uint16 *out_etypes,
                  uint32 max_frames, PkTrace *trace);

// Allocate a TX buffer from the context's pool (convenience wrapper)
static inline PktBuf *l2_alloc_buf(NetContext *ctx) {
    return pktbuf_alloc(&ctx->pool);
}

// Return a buffer to the context's pool (convenience wrapper)
static inline void l2_free_buf(NetContext *ctx, PktBuf *buf) {
    pktbuf_free(&ctx->pool, buf);
}

// Get stats snapshot
void l2_get_stats(NetContext *ctx, L2Stats *out);

#endif
