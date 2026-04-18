#ifndef ISURUS_NET_PKTBUF_H
#define ISURUS_NET_PKTBUF_H

#include "../types.h"
#include "eth.h"

// Pre-allocated packet buffer pool.
// Carved from 4KB pages at init time. No per-packet malloc.
// Each slot is a fixed-size buffer with ETH_HDR_LEN bytes of headroom
// so L2 can prepend the Ethernet header without copying.
//
// Layout of each slot:
//   [ETH_HDR_LEN headroom] [payload up to ETH_MTU] [padding to slot size]
//
// The caller writes payload starting at pktbuf_payload(). On TX,
// L2 writes the Ethernet header into the headroom in-place (zero-copy).
// On RX, the NIC writes the full frame starting at pktbuf_data().

#define PKTBUF_SLOT_SIZE    2048    // must be power of 2, >= ETH_FRAME_MAX + headroom
#define PKTBUF_SLOTS_PER_PAGE (4096 / PKTBUF_SLOT_SIZE)   // 2 slots per 4KB page

// A single buffer slot. Not a struct the caller sees directly --
// access through pktbuf_data/pktbuf_payload helpers.
typedef struct PktBuf {
    struct PktBuf *next;    // free list linkage (unused when allocated)
    uint16 data_off;        // offset from start of slot to frame data
    uint16 data_len;        // length of valid data
    uint16 pool_idx;        // which pool this belongs to (for returning)
    uint16 flags;           // reserved
    // The rest of the slot (up to PKTBUF_SLOT_SIZE - 16) is the data area.
    // Frame data starts at &buf[data_off] relative to this struct.
} PktBuf;

#define PKTBUF_HDR_SIZE   16    // size of PktBuf header (next + offsets)
#define PKTBUF_DATA_AREA  (PKTBUF_SLOT_SIZE - PKTBUF_HDR_SIZE)  // 2032 bytes

// A buffer pool. Allocated once, used for the lifetime of the NIC.
typedef struct {
    PktBuf *free_list;      // head of free slot chain
    uint8  *pages;          // base of allocated page range
    uint32  total_slots;    // total slots in pool
    uint32  free_count;     // current free slots
    uint32  page_count;     // number of 4KB pages backing the pool
} PktBufPool;

// Initialize a pool with n_pages * 4KB of memory.
// Uses alloc_pages (kernel) or a pre-mapped region (userland).
// Returns 0 on success, -1 on failure.
int pktbuf_pool_init(PktBufPool *pool, uint32 n_pages, uint8 *backing_memory);

// Allocate a buffer from the pool. Returns NULL if exhausted.
PktBuf *pktbuf_alloc(PktBufPool *pool);

// Return a buffer to the pool.
void pktbuf_free(PktBufPool *pool, PktBuf *buf);

// Pointer to the start of the data area (for RX: NIC writes full frame here).
static inline uint8 *pktbuf_data(PktBuf *buf) {
    return (uint8 *)buf + PKTBUF_HDR_SIZE;
}

// Pointer to payload area (headroom for ETH_HDR_LEN already reserved).
// For TX: caller writes payload here, then l2_send_zerocopy prepends the header.
static inline uint8 *pktbuf_payload(PktBuf *buf) {
    return (uint8 *)buf + PKTBUF_HDR_SIZE + ETH_HDR_LEN;
}

// Set the buffer to point at a received frame of given length.
static inline void pktbuf_set_rx(PktBuf *buf, uint32 frame_len) {
    buf->data_off = PKTBUF_HDR_SIZE;
    buf->data_len = (frame_len > PKTBUF_DATA_AREA) ? PKTBUF_DATA_AREA : frame_len;
}

// Set the buffer for TX with payload_len bytes starting after headroom.
static inline void pktbuf_set_tx(PktBuf *buf, uint32 payload_len) {
    buf->data_off = PKTBUF_HDR_SIZE;  // header will be written at data_off
    buf->data_len = ETH_HDR_LEN + payload_len;
}

// Pool stats
static inline uint32 pktbuf_pool_free(PktBufPool *pool) { return pool->free_count; }
static inline uint32 pktbuf_pool_total(PktBufPool *pool) { return pool->total_slots; }
static inline uint32 pktbuf_pool_used(PktBufPool *pool) {
    return pool->total_slots - pool->free_count;
}

#endif
