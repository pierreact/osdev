#include <net/virtio_net.h>
#include <kernel/mem.h>
#include <drivers/monitor.h>

#define barrier() __asm__ volatile("" ::: "memory")

#define VIRTIO_NET_HDR_SIZE sizeof(VirtioNetHdr)

static const char hex_chars[] = "0123456789abcdef";

static void print_hex8(uint8 val) {
    putc(hex_chars[(val >> 4) & 0xF]);
    putc(hex_chars[val & 0xF]);
}

// RX queue = 0, TX queue = 1
#define RXQ 0
#define TXQ 1

// Features we want from the device
#define DRIVER_FEATURES (VIRTIO_NET_F_MAC | VIRTIO_NET_F_STATUS)

static void read_mac(VirtioNetDevice *dev) {
    volatile uint8 *cfg = dev->vdev.device_cfg;
    if (!cfg)
        return;
    for (int i = 0; i < 6; i++)
        dev->mac[i] = cfg[i];
}

static void prefill_rx(VirtioNetDevice *dev) {
    Virtqueue *rxq = &dev->vdev.vqs[RXQ];
    uint16 count = rxq->size;
    if (count > VIRTIO_NET_RX_BUFFERS)
        count = VIRTIO_NET_RX_BUFFERS;

    for (uint16 i = 0; i < count; i++) {
        if (rxq->num_free == 0)
            break;

        // Take a free descriptor
        uint16 idx = rxq->free_head;
        rxq->free_head = rxq->desc[idx].next;
        rxq->num_free--;

        // Point descriptor at RX buffer
        uint8 *buf = dev->rx_buffers + (uint64)i * VIRTIO_NET_BUF_SIZE;
        rxq->desc[idx].addr = (uint64)buf;
        rxq->desc[idx].len = VIRTIO_NET_BUF_SIZE;
        rxq->desc[idx].flags = VIRTQ_DESC_F_WRITE;
        rxq->desc[idx].next = 0;

        // Add to available ring
        uint16 avail_idx = rxq->avail->idx;
        rxq->avail->ring[avail_idx % rxq->size] = idx;
        barrier();
        rxq->avail->idx = avail_idx + 1;
    }

    // Notify device about available RX buffers
    virtio_pci_notify(&dev->vdev, RXQ);
}

int virtio_net_init(VirtioNetDevice *dev, const PCIDevice *pci) {
    memset(dev, 0, sizeof(VirtioNetDevice));

    // Initialize virtio PCI device (reset, acknowledge, driver)
    if (virtio_pci_init_device(&dev->vdev, pci) != 0)
        return -1;

    // Negotiate features
    if (virtio_pci_negotiate_features(&dev->vdev, DRIVER_FEATURES) != 0)
        return -1;

    // Setup RX and TX queues
    if (virtio_pci_setup_queue(&dev->vdev, RXQ) != 0) {
        kprint("VIRTIO-NET: failed to setup RX queue\n");
        return -1;
    }
    if (virtio_pci_setup_queue(&dev->vdev, TXQ) != 0) {
        kprint("VIRTIO-NET: failed to setup TX queue\n");
        return -1;
    }

    // Allocate RX and TX buffer pools
    uint32 rx_pages = (VIRTIO_NET_RX_BUFFERS * VIRTIO_NET_BUF_SIZE + 4095) / 4096;
    uint32 tx_pages = (VIRTIO_NET_TX_BUFFERS * VIRTIO_NET_BUF_SIZE + 4095) / 4096;
    dev->rx_buffers = (uint8 *)alloc_pages(rx_pages);
    dev->tx_buffers = (uint8 *)alloc_pages(tx_pages);

    memset(dev->rx_buffers, 0, rx_pages * 4096);
    memset(dev->tx_buffers, 0, tx_pages * 4096);

    // Set DRIVER_OK to bring device live
    virtio_pci_set_driver_ok(&dev->vdev);

    // Read MAC address
    read_mac(dev);

    // Read link status
    dev->link_up = virtio_net_link_status(dev);

    kprint("VIRTIO-NET: MAC ");
    for (int i = 0; i < 6; i++) {
        print_hex8(dev->mac[i]);
        if (i < 5) putc(':');
    }
    kprint(dev->link_up ? " link up\n" : " link down\n");

    // Pre-fill RX queue with buffers
    prefill_rx(dev);

    return 0;
}

int virtio_net_send(VirtioNetDevice *dev, const uint8 *data, uint32 len) {
    Virtqueue *txq = &dev->vdev.vqs[TXQ];

    if (txq->num_free == 0)
        return -1;

    if (len + VIRTIO_NET_HDR_SIZE > VIRTIO_NET_BUF_SIZE)
        return -1;

    // Take a free descriptor
    uint16 idx = txq->free_head;
    txq->free_head = txq->desc[idx].next;
    txq->num_free--;

    // Prepare buffer: virtio-net header + packet data
    uint8 *buf = dev->tx_buffers + (uint64)idx * VIRTIO_NET_BUF_SIZE;
    memset(buf, 0, VIRTIO_NET_HDR_SIZE);
    memcpy(buf + VIRTIO_NET_HDR_SIZE, data, len);

    txq->desc[idx].addr = (uint64)buf;
    txq->desc[idx].len = VIRTIO_NET_HDR_SIZE + len;
    txq->desc[idx].flags = 0;
    txq->desc[idx].next = 0;

    // Add to available ring
    uint16 avail_idx = txq->avail->idx;
    txq->avail->ring[avail_idx % txq->size] = idx;
    barrier();
    txq->avail->idx = avail_idx + 1;

    // Notify device
    virtio_pci_notify(&dev->vdev, TXQ);

    // Poll for completion (BSP management traffic is low-rate)
    while (txq->last_used_idx == txq->used->idx)
        barrier();

    // Reclaim descriptor
    uint32 used_idx = txq->last_used_idx % txq->size;
    uint32 used_id = txq->used->ring[used_idx].id;
    txq->desc[used_id].next = txq->free_head;
    txq->desc[used_id].flags = VIRTQ_DESC_F_NEXT;
    txq->free_head = used_id;
    txq->num_free++;
    txq->last_used_idx++;

    return 0;
}

int virtio_net_recv(VirtioNetDevice *dev, uint8 *buf, uint32 *len) {
    Virtqueue *rxq = &dev->vdev.vqs[RXQ];

    // Check if device has placed any packets in used ring
    if (rxq->last_used_idx == rxq->used->idx)
        return -1;  // no packet available

    barrier();

    uint16 used_idx = rxq->last_used_idx % rxq->size;
    uint32 desc_id = rxq->used->ring[used_idx].id;
    uint32 total_len = rxq->used->ring[used_idx].len;

    // Skip virtio-net header
    if (total_len <= VIRTIO_NET_HDR_SIZE) {
        // Repost and skip
        goto repost;
    }

    uint32 pkt_len = total_len - VIRTIO_NET_HDR_SIZE;
    uint8 *src = (uint8 *)(uint64)rxq->desc[desc_id].addr + VIRTIO_NET_HDR_SIZE;
    memcpy(buf, src, pkt_len);
    *len = pkt_len;

repost:
    // Repost descriptor for reuse
    rxq->desc[desc_id].addr = (uint64)(dev->rx_buffers + (uint64)desc_id * VIRTIO_NET_BUF_SIZE);
    rxq->desc[desc_id].len = VIRTIO_NET_BUF_SIZE;
    rxq->desc[desc_id].flags = VIRTQ_DESC_F_WRITE;
    rxq->desc[desc_id].next = 0;

    uint16 avail_idx = rxq->avail->idx;
    rxq->avail->ring[avail_idx % rxq->size] = desc_id;
    barrier();
    rxq->avail->idx = avail_idx + 1;

    rxq->last_used_idx++;

    // Notify device
    virtio_pci_notify(&dev->vdev, RXQ);

    return (total_len > VIRTIO_NET_HDR_SIZE) ? 0 : -1;
}

void virtio_net_get_mac(VirtioNetDevice *dev, uint8 *mac_out) {
    memcpy(mac_out, dev->mac, 6);
}

int virtio_net_link_status(VirtioNetDevice *dev) {
    volatile uint8 *cfg = dev->vdev.device_cfg;
    if (!cfg)
        return 1;  // assume up if no status capability
    // Link status is at offset 6 in device-specific config (after MAC)
    volatile uint16 *status = (volatile uint16 *)(cfg + 6);
    return (*status & VIRTIO_NET_S_LINK_UP) ? 1 : 0;
}
