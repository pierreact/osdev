#include <net/nic.h>
#include <drivers/pci.h>
#include <net/virtio_net.h>
#include <drivers/monitor.h>

static NICSlot nics[MAX_NICS];
static uint32 nic_count = 0;

// Static storage for virtio-net devices
static VirtioNetDevice virtio_net_devs[MAX_NICS];

// Wrappers to cast void* dev back to VirtioNetDevice*
static int vnet_send(void *dev, const uint8 *data, uint32 len) {
    return virtio_net_send((VirtioNetDevice *)dev, data, len);
}

static int vnet_recv(void *dev, uint8 *buf, uint32 *len) {
    return virtio_net_recv((VirtioNetDevice *)dev, buf, len);
}

static void vnet_get_mac(void *dev, uint8 *mac_out) {
    virtio_net_get_mac((VirtioNetDevice *)dev, mac_out);
}

static int vnet_link_status(void *dev) {
    return virtio_net_link_status((VirtioNetDevice *)dev);
}

static const NICOps virtio_net_ops = {
    .send = vnet_send,
    .recv = vnet_recv,
    .get_mac = vnet_get_mac,
    .link_status = vnet_link_status,
};

static const char *vnet_names[] = {
    "virtio-net0", "virtio-net1", "virtio-net2", "virtio-net3"
};

void nic_init(void) {
    nic_count = 0;
    for (uint32 i = 0; i < MAX_NICS; i++)
        nics[i].active = 0;

    uint32 dev_count = pci_get_device_count();

    for (uint32 i = 0; i < dev_count && nic_count < MAX_NICS; i++) {
        const PCIDevice *pci = pci_get_device(i);

        // Check for virtio-net (legacy or modern)
        if (pci->vendor_id == VIRTIO_PCI_VENDOR &&
            (pci->device_id == VIRTIO_NET_DEVICE_LEGACY ||
             pci->device_id == VIRTIO_NET_DEVICE_MODERN)) {

            VirtioNetDevice *vnet = &virtio_net_devs[nic_count];
            if (virtio_net_init(vnet, pci) != 0) {
                kprint("NIC: failed to init virtio-net at ");
                kprint_dec(pci->bus);
                putc(':');
                kprint_dec(pci->dev);
                putc('.');
                kprint_dec(pci->func);
                putc('\n');
                continue;
            }

            NICSlot *slot = &nics[nic_count];
            slot->name = vnet_names[nic_count];
            slot->ops = virtio_net_ops;
            slot->dev = vnet;
            slot->active = 1;
            nic_count++;
        }
    }

    kprint("NIC: ");
    kprint_dec(nic_count);
    kprint(" interface(s) up\n");
}

int nic_send(uint32 idx, const uint8 *data, uint32 len) {
    if (idx >= nic_count || !nics[idx].active)
        return -1;
    return nics[idx].ops.send(nics[idx].dev, data, len);
}

int nic_recv(uint32 idx, uint8 *buf, uint32 *len) {
    if (idx >= nic_count || !nics[idx].active)
        return -1;
    return nics[idx].ops.recv(nics[idx].dev, buf, len);
}

void nic_get_mac(uint32 idx, uint8 *mac_out) {
    if (idx >= nic_count || !nics[idx].active)
        return;
    nics[idx].ops.get_mac(nics[idx].dev, mac_out);
}

int nic_link_status(uint32 idx) {
    if (idx >= nic_count || !nics[idx].active)
        return 0;
    return nics[idx].ops.link_status(nics[idx].dev);
}

uint32 nic_get_count(void) {
    return nic_count;
}

const char *nic_name(uint32 idx) {
    if (idx >= nic_count)
        return NULL;
    return nics[idx].name;
}
