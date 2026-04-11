#ifndef SYSTEM_VIRTIO_H
#define SYSTEM_VIRTIO_H

#include <types.h>
#include <drivers/pci.h>

// Virtio PCI vendor
#define VIRTIO_PCI_VENDOR       0x1AF4

// Virtio PCI capability types
#define VIRTIO_PCI_CAP_COMMON_CFG   1
#define VIRTIO_PCI_CAP_NOTIFY_CFG   2
#define VIRTIO_PCI_CAP_ISR_CFG      3
#define VIRTIO_PCI_CAP_DEVICE_CFG   4
#define VIRTIO_PCI_CAP_PCI_CFG      5

// Virtio device status bits
#define VIRTIO_STATUS_ACKNOWLEDGE   1
#define VIRTIO_STATUS_DRIVER        2
#define VIRTIO_STATUS_FEATURES_OK   8
#define VIRTIO_STATUS_DRIVER_OK     4
#define VIRTIO_STATUS_FAILED        128

// Virtqueue descriptor flags
#define VIRTQ_DESC_F_NEXT       1
#define VIRTQ_DESC_F_WRITE      2
#define VIRTQ_DESC_F_INDIRECT   4

// Common config register offsets (virtio 1.x PCI)
#define VIRTIO_COMMON_DFSELECT      0x00  // uint32
#define VIRTIO_COMMON_DF            0x04  // uint32
#define VIRTIO_COMMON_GFSELECT      0x08  // uint32
#define VIRTIO_COMMON_GF            0x0C  // uint32
#define VIRTIO_COMMON_MSIX          0x10  // uint16
#define VIRTIO_COMMON_NUMQ          0x12  // uint16
#define VIRTIO_COMMON_STATUS        0x14  // uint8
#define VIRTIO_COMMON_CFGGEN        0x15  // uint8
#define VIRTIO_COMMON_Q_SELECT      0x16  // uint16
#define VIRTIO_COMMON_Q_SIZE        0x18  // uint16
#define VIRTIO_COMMON_Q_MSIX        0x1A  // uint16
#define VIRTIO_COMMON_Q_ENABLE      0x1C  // uint16
#define VIRTIO_COMMON_Q_NOTIFY_OFF  0x1E  // uint16
#define VIRTIO_COMMON_Q_DESC        0x20  // uint64
#define VIRTIO_COMMON_Q_AVAIL       0x28  // uint64
#define VIRTIO_COMMON_Q_USED        0x30  // uint64

#define VIRTQ_MAX_SIZE 256

typedef struct __attribute__((packed)) {
    uint64 addr;
    uint32 len;
    uint16 flags;
    uint16 next;
} VirtqDesc;

typedef struct __attribute__((packed)) {
    uint16 flags;
    uint16 idx;
    uint16 ring[VIRTQ_MAX_SIZE];
} VirtqAvail;

typedef struct __attribute__((packed)) {
    uint32 id;
    uint32 len;
} VirtqUsedElem;

typedef struct __attribute__((packed)) {
    uint16 flags;
    uint16 idx;
    VirtqUsedElem ring[VIRTQ_MAX_SIZE];
} VirtqUsed;

typedef struct {
    uint16      size;
    VirtqDesc  *desc;
    VirtqAvail *avail;
    VirtqUsed  *used;
    uint16      free_head;
    uint16      num_free;
    uint16      last_used_idx;
} Virtqueue;

#define VIRTIO_MAX_QUEUES 4

typedef struct {
    const PCIDevice *pci;
    volatile uint8  *common_cfg;
    volatile uint8  *notify_base;
    uint32           notify_off_multiplier;
    volatile uint8  *isr_cfg;
    volatile uint8  *device_cfg;
    Virtqueue        vqs[VIRTIO_MAX_QUEUES];
    uint8            num_vqs;
} VirtioPCIDevice;

int  virtio_pci_init_device(VirtioPCIDevice *vdev, const PCIDevice *pci);
int  virtio_pci_negotiate_features(VirtioPCIDevice *vdev, uint64 driver_features);
int  virtio_pci_setup_queue(VirtioPCIDevice *vdev, uint16 queue_idx);
void virtio_pci_notify(VirtioPCIDevice *vdev, uint16 queue_idx);
int  virtio_pci_set_driver_ok(VirtioPCIDevice *vdev);

#endif
