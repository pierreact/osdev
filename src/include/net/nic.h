#ifndef SYSTEM_NIC_H
#define SYSTEM_NIC_H

#include <types.h>

#define MAX_NICS 4

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
} NICSlot;

void        nic_init(void);
int         nic_send(uint32 idx, const uint8 *data, uint32 len);
int         nic_recv(uint32 idx, uint8 *buf, uint32 *len);
void        nic_get_mac(uint32 idx, uint8 *mac_out);
int         nic_link_status(uint32 idx);
uint32      nic_get_count(void);
const char *nic_name(uint32 idx);

#endif
