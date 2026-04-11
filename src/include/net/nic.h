#ifndef SYSTEM_NIC_H
#define SYSTEM_NIC_H

#include <types.h>

#define MAX_NICS 4
#define NIC_NONE 0xFFFFFFFFu

// First N NICs (in enumeration order) are reserved as BSP NICs
// (mgmt + inter-node) and excluded from the AP assignment pool.
#define BSP_NIC_COUNT 2

typedef enum {
    NIC_MODE_PER_NUMA = 0,    // one NIC per NUMA node, shared across cores
    NIC_MODE_PER_CORE = 1,    // one NIC per core, locality-respecting
} NicAssignmentMode;

typedef struct {
    int  (*send)(void *dev, const uint8 *data, uint32 len);
    int  (*recv)(void *dev, uint8 *buf, uint32 *len);
    void (*get_mac)(void *dev, uint8 *mac_out);
    int  (*link_status)(void *dev);
} NICOps;

typedef struct {
    const char *name;
    NICOps      ops;
    void       *dev;
    uint8       active;
    uint32      numa_node;     // proximity domain, or PCI_NUMA_UNKNOWN
} NICSlot;

void        nic_init(void);
int         nic_send(uint32 idx, const uint8 *data, uint32 len);
int         nic_recv(uint32 idx, uint8 *buf, uint32 *len);
void        nic_get_mac(uint32 idx, uint8 *mac_out);
int         nic_link_status(uint32 idx);
uint32      nic_get_count(void);
const char *nic_name(uint32 idx);
uint32      nic_get_numa_node(uint32 idx);
int         nic_find_for_node(uint32 node);

void                nic_set_mode(NicAssignmentMode mode);
NicAssignmentMode   nic_get_mode(void);
void                nic_assign(void);   // (re)compute per-CPU assignment

#endif
