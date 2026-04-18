#ifndef ISURUS_NET_ARP_H
#define ISURUS_NET_ARP_H

#include "../types.h"
#include "eth.h"

#define ARP_TABLE_SIZE  32
#define ARP_HW_ETHER    1
#define ARP_OP_REQUEST   1
#define ARP_OP_REPLY     2

typedef struct __attribute__((packed)) {
    uint16 hw_type;                     // hardware type (1 = Ethernet)
    uint16 proto_type;                  // protocol type (0x0800 = IPv4)
    uint8  hw_len;                      // hardware addr length (6)
    uint8  proto_len;                   // protocol addr length (4)
    uint16 opcode;                      // ARP_OP_REQUEST or ARP_OP_REPLY
    uint8  sender_mac[ETH_ADDR_LEN];
    uint32 sender_ip;                   // network byte order
    uint8  target_mac[ETH_ADDR_LEN];
    uint32 target_ip;                   // network byte order
} ArpPacket;

#define ARP_PKT_LEN  sizeof(ArpPacket)  // 28 bytes

typedef struct {
    uint32 ip;                          // network byte order, 0 = empty slot
    uint8  mac[ETH_ADDR_LEN];
    uint8  valid;
    uint8  reserved;
} ArpEntry;

typedef struct {
    ArpEntry entries[ARP_TABLE_SIZE];
    uint32   count;
} ArpTable;

// Initialize ARP table (zero all entries)
void arp_table_init(ArpTable *t);

// Look up IP in table. Returns pointer to MAC if found, NULL if not.
const uint8 *arp_lookup(ArpTable *t, uint32 ip);

// Insert or update an entry
void arp_insert(ArpTable *t, uint32 ip, const uint8 *mac);

// Process an incoming ARP frame (full Ethernet frame including EthHdr).
// Handles requests (sends reply if for our IP) and replies (updates table).
// l2ctx is an opaque L2Context pointer.
int arp_process(void *l2ctx, ArpTable *t, uint8 *frame, uint32 frame_len);

// Send an ARP request for the given IP
int arp_request(void *l2ctx, uint32 target_ip);

#endif
