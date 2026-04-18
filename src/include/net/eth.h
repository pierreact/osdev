#ifndef ISURUS_NET_ETH_H
#define ISURUS_NET_ETH_H

#include <types.h>

#define ETH_ADDR_LEN    6
#define ETH_HDR_LEN     14
#define ETH_MTU         1500
#define ETH_FRAME_MAX   (ETH_HDR_LEN + ETH_MTU)   // 1514

// Standard ethertypes
#define ETH_TYPE_IPV4   0x0800
#define ETH_TYPE_ARP    0x0806

// Custom ethertype for inter-node DSM (IEEE local experimental)
#define ETH_TYPE_DSM    0x88B5

typedef struct __attribute__((packed)) {
    uint8  dst[ETH_ADDR_LEN];
    uint8  src[ETH_ADDR_LEN];
    uint16 ethertype;               // network byte order
} EthHdr;

// Broadcast MAC
extern const uint8 ETH_BROADCAST[ETH_ADDR_LEN];

// Byte-order helpers
static inline uint16 htons(uint16 v) { return (v >> 8) | (v << 8); }
static inline uint16 ntohs(uint16 v) { return htons(v); }
static inline uint32 htonl(uint32 v) {
    return ((v >> 24) & 0xFF) | ((v >> 8) & 0xFF00)
         | ((v << 8) & 0xFF0000) | ((v << 24) & 0xFF000000u);
}
static inline uint32 ntohl(uint32 v) { return htonl(v); }

// Parse: returns pointer to EthHdr if frame >= ETH_HDR_LEN, else NULL
static inline EthHdr *eth_hdr(uint8 *frame, uint32 len) {
    if (len < ETH_HDR_LEN) return (EthHdr *)0;
    return (EthHdr *)frame;
}

// Payload pointer and length
static inline uint8 *eth_payload(uint8 *frame) {
    return frame + ETH_HDR_LEN;
}

static inline uint32 eth_payload_len(uint32 frame_len) {
    return (frame_len > ETH_HDR_LEN) ? frame_len - ETH_HDR_LEN : 0;
}

// Compare two MAC addresses
static inline int eth_mac_eq(const uint8 *a, const uint8 *b) {
    return a[0]==b[0] && a[1]==b[1] && a[2]==b[2]
        && a[3]==b[3] && a[4]==b[4] && a[5]==b[5];
}

// Check if MAC is broadcast (FF:FF:FF:FF:FF:FF)
static inline int eth_is_broadcast(const uint8 *mac) {
    return mac[0]==0xFF && mac[1]==0xFF && mac[2]==0xFF
        && mac[3]==0xFF && mac[4]==0xFF && mac[5]==0xFF;
}

// Build an Ethernet header at buf[0..13].
// Returns ETH_HDR_LEN (14).
int eth_build_hdr(uint8 *buf, const uint8 *dst, const uint8 *src, uint16 ethertype);

#endif
