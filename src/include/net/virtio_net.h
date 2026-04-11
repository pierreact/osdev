#ifndef SYSTEM_VIRTIO_NET_H
#define SYSTEM_VIRTIO_NET_H

#include <types.h>
#include <net/virtio.h>

// Virtio-net device IDs
#define VIRTIO_NET_DEVICE_LEGACY    0x1000
#define VIRTIO_NET_DEVICE_MODERN    0x1041

// Virtio-net feature bits
#define VIRTIO_NET_F_MAC            (1UL << 5)
#define VIRTIO_NET_F_STATUS         (1UL << 16)
#define VIRTIO_NET_F_MRG_RXBUF     (1UL << 15)

// Link status (from device config)
#define VIRTIO_NET_S_LINK_UP        1

#define VIRTIO_NET_RX_BUFFERS       128
#define VIRTIO_NET_TX_BUFFERS       128
#define VIRTIO_NET_BUF_SIZE         2048

typedef struct __attribute__((packed)) {
    uint8  flags;
    uint8  gso_type;
    uint16 hdr_len;
    uint16 gso_size;
    uint16 csum_start;
    uint16 csum_offset;
    uint16 num_buffers;
} VirtioNetHdr;

typedef struct {
    VirtioPCIDevice vdev;
    uint8   mac[6];
    uint8  *rx_buffers;
    uint8  *tx_buffers;
    uint8   link_up;
} VirtioNetDevice;

int  virtio_net_init(VirtioNetDevice *dev, const PCIDevice *pci);
int  virtio_net_send(VirtioNetDevice *dev, const uint8 *data, uint32 len);
int  virtio_net_recv(VirtioNetDevice *dev, uint8 *buf, uint32 *len);
void virtio_net_get_mac(VirtioNetDevice *dev, uint8 *mac_out);
int  virtio_net_link_status(VirtioNetDevice *dev);

#endif
