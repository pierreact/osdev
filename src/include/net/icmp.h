#ifndef ISURUS_NET_ICMP_H
#define ISURUS_NET_ICMP_H

#include <types.h>
#include <net/l2.h>
#include <net/pktrace.h>

#define ICMP_TYPE_ECHO_REPLY    0
#define ICMP_TYPE_ECHO_REQUEST  8

// ICMP Echo header (the 8-byte fixed portion of an ICMP message as
// used for Echo / Echo Reply; payload follows).
typedef struct __attribute__((packed)) {
    uint8  type;
    uint8  code;
    uint16 checksum;         // network byte order, covers ICMP msg
    uint16 id;               // network byte order
    uint16 seq;              // network byte order
} IcmpEchoHdr;

#define ICMP_ECHO_HDR_LEN  sizeof(IcmpEchoHdr)   // 8 bytes

// Process an ICMP message addressed to us. Returns 0 on handled,
// -1 on drop. msg points to the ICMP payload of an IPv4 datagram
// (after the IPv4 header). src_ip is the sender (network byte order).
int icmp_rx(NetContext *ctx, uint32 src_ip, uint8 *msg, uint32 msg_len,
            PkTrace *trace);

// Send an ICMP Echo Request to dst_ip with the given id/seq and
// optional payload (NULL + 0 for a bare ping). Returns 0 on TX, -1
// on drop (ARP pending, oversize, etc.). Used by sys.net.ping.
int icmp_send_echo(NetContext *ctx, uint32 dst_ip, uint16 id, uint16 seq,
                   const uint8 *payload, uint32 payload_len,
                   PkTrace *trace);

#endif
