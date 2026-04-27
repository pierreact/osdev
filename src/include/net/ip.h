#ifndef ISURUS_NET_IP_H
#define ISURUS_NET_IP_H

#include <types.h>
#include <net/eth.h>
#include <net/l2.h>
#include <net/pktrace.h>

#define IPV4_VERSION        4
#define IPV4_IHL_MIN        5           // 5 * 4 = 20 bytes (no options)
#define IPV4_HDR_LEN        20          // IHL=5 header length
#define IPV4_DEFAULT_TTL    64

// Protocol numbers (IANA)
#define IP_PROTO_ICMP       1
#define IP_PROTO_TCP        6
#define IP_PROTO_UDP        17

// Flags field: upper 3 bits of the 16-bit frag_off word
#define IPV4_FLAG_DF        0x4000      // Don't Fragment
#define IPV4_FLAG_MF        0x2000      // More Fragments
#define IPV4_FRAG_OFF_MASK  0x1FFF      // fragment offset in 8-byte units

typedef struct __attribute__((packed)) {
    uint8  ver_ihl;          // upper nibble = version, lower = IHL
    uint8  tos;
    uint16 total_len;        // network byte order
    uint16 id;               // network byte order
    uint16 flags_frag;       // network byte order; top 3 bits flags
    uint8  ttl;
    uint8  proto;
    uint16 checksum;         // network byte order
    uint32 src_ip;           // network byte order
    uint32 dst_ip;           // network byte order
} Ipv4Hdr;

// RFC 1071 one's complement checksum over buf[0..len-1].
// len must be even; for odd lengths caller must zero-pad.
uint16 ip_checksum(const void *buf, uint32 len);

// Receive path: the payload/len passed in is the Ethernet payload
// (not including EthHdr). Validates, then either answers internally
// (ICMP Echo), forwards (if configured), or drops.
// Returns 0 if the frame was consumed (delivered/replied/forwarded),
// -1 on drop. No payload is handed back up; L3 and above live
// inside the L2/IP context.
int ip_rx(NetContext *ctx, uint8 *payload, uint32 payload_len, PkTrace *trace);

// Build an IPv4 header at buf (IPV4_HDR_LEN bytes) with the given
// protocol, src/dst, payload_len (payload size after the IP header).
// Fills total_len, checksum, and returns IPV4_HDR_LEN.
int ip_build_hdr(uint8 *buf, uint32 src_ip, uint32 dst_ip,
                 uint8 proto, uint16 payload_len);

// Send an IPv4 datagram: builds the header in a local buffer,
// prepends it to payload, resolves the next hop via ARP, and hands
// the frame to L2. Returns 0 on TX, -1 on drop (oversize, ARP
// pending, etc.).
int ip_send(NetContext *ctx, uint32 dst_ip, uint8 proto,
            const uint8 *payload, uint32 payload_len, PkTrace *trace);

// Copy the IP stats out.
void ip_get_stats(NetContext *ctx, IpStats *out);

#endif
