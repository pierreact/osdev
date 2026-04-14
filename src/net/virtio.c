#include <net/virtio.h>
#include <drivers/pci.h>
#include <kernel/mem.h>
#include <drivers/monitor.h>


// Compiler barrier (x86-64 TSO makes this sufficient for most virtio operations)
#define barrier() __asm__ volatile("" ::: "memory")

// Read/write helpers for common config
static uint8 ccfg_read8(VirtioPCIDevice *vdev, uint16 off) {
    return *(volatile uint8 *)(vdev->common_cfg + off);
}

static uint16 ccfg_read16(VirtioPCIDevice *vdev, uint16 off) {
    return *(volatile uint16 *)(vdev->common_cfg + off);
}

static uint32 ccfg_read32(VirtioPCIDevice *vdev, uint16 off) {
    return *(volatile uint32 *)(vdev->common_cfg + off);
}

static void ccfg_write8(VirtioPCIDevice *vdev, uint16 off, uint8 val) {
    *(volatile uint8 *)(vdev->common_cfg + off) = val;
}

static void ccfg_write16(VirtioPCIDevice *vdev, uint16 off, uint16 val) {
    *(volatile uint16 *)(vdev->common_cfg + off) = val;
}

static void ccfg_write32(VirtioPCIDevice *vdev, uint16 off, uint32 val) {
    *(volatile uint32 *)(vdev->common_cfg + off) = val;
}

static void ccfg_write64(VirtioPCIDevice *vdev, uint16 off, uint64 val) {
    *(volatile uint64 *)(vdev->common_cfg + off) = val;
}

// Walk PCI capability list to find virtio structure pointers
static int find_virtio_caps(VirtioPCIDevice *vdev) {
    const PCIDevice *pci = vdev->pci;

    uint16 status = pci_config_read16(pci, 0x06);
    if (!(status & (1 << 4)))  // capabilities list bit
        return -1;

    uint8 cap_offset = pci_config_read8(pci, PCI_CAP_PTR) & 0xFC;

    vdev->common_cfg = NULL;
    vdev->notify_base = NULL;
    vdev->isr_cfg = NULL;
    vdev->device_cfg = NULL;
    vdev->notify_off_multiplier = 0;

    while (cap_offset) {
        uint8 cap_id = pci_config_read8(pci, cap_offset);
        uint8 cap_next = pci_config_read8(pci, cap_offset + 1);

        // Vendor-specific capability (0x09) is what virtio uses
        if (cap_id == 0x09) {
            uint8 cfg_type = pci_config_read8(pci, cap_offset + 3);
            uint8 bar_idx  = pci_config_read8(pci, cap_offset + 4);
            uint32 bar_off = pci_config_read32(pci, cap_offset + 8);

            if (bar_idx >= 6 || !pci->bar_is_mmio[bar_idx])
                goto next;

            volatile uint8 *base = (volatile uint8 *)pci->bar[bar_idx] + bar_off;

            switch (cfg_type) {
            case VIRTIO_PCI_CAP_COMMON_CFG:
                vdev->common_cfg = base;
                break;
            case VIRTIO_PCI_CAP_NOTIFY_CFG:
                vdev->notify_base = base;
                vdev->notify_off_multiplier = pci_config_read32(pci, cap_offset + 16);
                break;
            case VIRTIO_PCI_CAP_ISR_CFG:
                vdev->isr_cfg = base;
                break;
            case VIRTIO_PCI_CAP_DEVICE_CFG:
                vdev->device_cfg = base;
                break;
            }
        }

next:
        cap_offset = cap_next & 0xFC;
    }

    if (!vdev->common_cfg || !vdev->notify_base) {
        kprint("VIRTIO: missing required capabilities\n");
        return -1;
    }

    return 0;
}

// Map all MMIO BARs used by the device
static void map_device_bars(const PCIDevice *pci) {
    for (int i = 0; i < 6; i++) {
        if (pci->bar[i] && pci->bar_is_mmio[i]) {
            // Map at least 4KB per BAR (most virtio BARs are small)
            // For larger BARs we'd need to read the BAR size; 64KB is safe
            map_mmio_range(pci->bar[i], 0x10000);
        }
    }
}

int virtio_pci_init_device(VirtioPCIDevice *vdev, const PCIDevice *pci) {
    vdev->pci = pci;
    vdev->num_vqs = 0;

    // Enable bus mastering and MMIO access
    uint16 cmd = pci_config_read16(pci, 0x04);
    cmd |= PCI_COMMAND_BUS_MASTER | PCI_COMMAND_MMIO;
    pci_config_write16(pci, 0x04, cmd);

    // Map BARs
    map_device_bars(pci);

    // Find virtio PCI capabilities
    if (find_virtio_caps(vdev) != 0)
        return -1;

    // Reset device
    ccfg_write8(vdev, VIRTIO_COMMON_STATUS, 0);
    barrier();

    // Wait for reset (read back 0)
    while (ccfg_read8(vdev, VIRTIO_COMMON_STATUS) != 0)
        ;

    // Set ACKNOWLEDGE
    ccfg_write8(vdev, VIRTIO_COMMON_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);
    barrier();

    // Set DRIVER
    uint8 s = ccfg_read8(vdev, VIRTIO_COMMON_STATUS);
    ccfg_write8(vdev, VIRTIO_COMMON_STATUS, s | VIRTIO_STATUS_DRIVER);
    barrier();

    return 0;
}

int virtio_pci_negotiate_features(VirtioPCIDevice *vdev, uint64 driver_features) {
    // Read device features (low 32 bits)
    ccfg_write32(vdev, VIRTIO_COMMON_DFSELECT, 0);
    barrier();
    uint32 dev_lo = ccfg_read32(vdev, VIRTIO_COMMON_DF);

    // Read device features (high 32 bits)
    ccfg_write32(vdev, VIRTIO_COMMON_DFSELECT, 1);
    barrier();
    uint32 dev_hi = ccfg_read32(vdev, VIRTIO_COMMON_DF);

    uint64 device_features = ((uint64)dev_hi << 32) | dev_lo;
    uint64 accepted = device_features & driver_features;

    // Write accepted features
    ccfg_write32(vdev, VIRTIO_COMMON_GFSELECT, 0);
    barrier();
    ccfg_write32(vdev, VIRTIO_COMMON_GF, (uint32)(accepted & 0xFFFFFFFF));

    ccfg_write32(vdev, VIRTIO_COMMON_GFSELECT, 1);
    barrier();
    ccfg_write32(vdev, VIRTIO_COMMON_GF, (uint32)(accepted >> 32));

    // Set FEATURES_OK
    uint8 s = ccfg_read8(vdev, VIRTIO_COMMON_STATUS);
    ccfg_write8(vdev, VIRTIO_COMMON_STATUS, s | VIRTIO_STATUS_FEATURES_OK);
    barrier();

    // Verify device accepted
    s = ccfg_read8(vdev, VIRTIO_COMMON_STATUS);
    if (!(s & VIRTIO_STATUS_FEATURES_OK)) {
        kprint("VIRTIO: feature negotiation failed\n");
        ccfg_write8(vdev, VIRTIO_COMMON_STATUS, VIRTIO_STATUS_FAILED);
        return -1;
    }

    return 0;
}

int virtio_pci_setup_queue(VirtioPCIDevice *vdev, uint16 queue_idx) {
    if (queue_idx >= VIRTIO_MAX_QUEUES)
        return -1;

    // Select queue
    ccfg_write16(vdev, VIRTIO_COMMON_Q_SELECT, queue_idx);
    barrier();

    uint16 size = ccfg_read16(vdev, VIRTIO_COMMON_Q_SIZE);
    if (size == 0)
        return -1;
    if (size > VIRTQ_MAX_SIZE)
        size = VIRTQ_MAX_SIZE;

    // Allocate descriptor table: size * 16 bytes
    uint32 desc_pages = (size * sizeof(VirtqDesc) + 4095) / 4096;
    uint64 desc_phys = alloc_pages(desc_pages);

    // Allocate available ring: 4 + 2*size bytes
    uint32 avail_pages = (4 + 2 * size + 4095) / 4096;
    uint64 avail_phys = alloc_pages(avail_pages);

    // Allocate used ring: 4 + 8*size bytes
    uint32 used_pages = (4 + 8 * size + 4095) / 4096;
    uint64 used_phys = alloc_pages(used_pages);

    // Zero all memory
    memset((void *)desc_phys, 0, desc_pages * 4096);
    memset((void *)avail_phys, 0, avail_pages * 4096);
    memset((void *)used_phys, 0, used_pages * 4096);

    // Initialize virtqueue state
    Virtqueue *vq = &vdev->vqs[queue_idx];
    vq->size = size;
    vq->desc = (VirtqDesc *)desc_phys;
    vq->avail = (VirtqAvail *)avail_phys;
    vq->used = (VirtqUsed *)used_phys;
    vq->free_head = 0;
    vq->num_free = size;
    vq->last_used_idx = 0;

    // Build free descriptor chain
    for (uint16 i = 0; i < size - 1; i++) {
        vq->desc[i].next = i + 1;
        vq->desc[i].flags = VIRTQ_DESC_F_NEXT;
    }
    vq->desc[size - 1].next = 0;
    vq->desc[size - 1].flags = 0;

    // Tell device about queue layout
    ccfg_write16(vdev, VIRTIO_COMMON_Q_SIZE, size);
    barrier();
    ccfg_write64(vdev, VIRTIO_COMMON_Q_DESC, desc_phys);
    ccfg_write64(vdev, VIRTIO_COMMON_Q_AVAIL, avail_phys);
    ccfg_write64(vdev, VIRTIO_COMMON_Q_USED, used_phys);
    barrier();

    // Enable queue
    ccfg_write16(vdev, VIRTIO_COMMON_Q_ENABLE, 1);
    barrier();

    if (queue_idx >= vdev->num_vqs)
        vdev->num_vqs = queue_idx + 1;

    return 0;
}

void virtio_pci_notify(VirtioPCIDevice *vdev, uint16 queue_idx) {
    // Read the queue's notify offset
    ccfg_write16(vdev, VIRTIO_COMMON_Q_SELECT, queue_idx);
    barrier();
    uint16 notify_off = ccfg_read16(vdev, VIRTIO_COMMON_Q_NOTIFY_OFF);

    volatile uint16 *notify_addr = (volatile uint16 *)(
        vdev->notify_base + (uint64)notify_off * vdev->notify_off_multiplier
    );

    __asm__ volatile("sfence" ::: "memory");
    *notify_addr = queue_idx;
}

int virtio_pci_set_driver_ok(VirtioPCIDevice *vdev) {
    uint8 s = ccfg_read8(vdev, VIRTIO_COMMON_STATUS);
    ccfg_write8(vdev, VIRTIO_COMMON_STATUS, s | VIRTIO_STATUS_DRIVER_OK);
    barrier();
    return 0;
}
