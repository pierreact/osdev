#include <net/nic.h>
#include <drivers/pci.h>
#include <net/virtio.h>
#include <net/virtio_net.h>
#include <drivers/monitor.h>
#include <kernel/cpu.h>
#include <arch/acpi.h>
#include <arch/apic.h>

static NICSlot nics[MAX_NICS];
static uint32 nic_count = 0;
static NicAssignmentMode current_mode = NIC_MODE_PER_NUMA;

// Static storage for virtio-net devices
static VirtioNetDevice virtio_net_devs[MAX_NICS];

// Forward declaration
static NicAssignmentMode nic_choose_default_mode(void);

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

// Per-NIC name buffer, generated at init from driver type + global index
static char nic_name_bufs[MAX_NICS][16];

static void gen_nic_name(uint32 idx, const char *prefix) {
    char *buf = nic_name_bufs[idx];
    uint32 i = 0;
    while (*prefix && i < 12) buf[i++] = *prefix++;
    // Append decimal index
    if (idx >= 100) buf[i++] = '0' + (idx / 100) % 10;
    if (idx >= 10) buf[i++] = '0' + (idx / 10) % 10;
    buf[i++] = '0' + idx % 10;
    buf[i] = '\0';
}

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
            gen_nic_name(nic_count, "virtio-net");
            slot->name = nic_name_bufs[nic_count];
            slot->ops = virtio_net_ops;
            slot->dev = vnet;
            slot->active = 1;
            slot->numa_node = pci->numa_node;
            nic_count++;
        }
    }

    kprint("NIC: ");
    kprint_dec(nic_count);
    kprint(" interface(s) up\n");

    current_mode = nic_choose_default_mode();
    nic_assign();
    kprint("NIC: assignment mode ");
    kprint(current_mode == NIC_MODE_PER_CORE ? "per-core" : "per-numa");
    kprint("\n");

    // Dump NIC pool and per-CPU assignment
    for (uint32 i = 0; i < nic_count; i++) {
        kprint("NIC: slot ");
        kprint_dec(i);
        kprint(" NUMA ");
        if (nics[i].numa_node == PCI_NUMA_UNKNOWN) kprint("-");
        else kprint_dec(nics[i].numa_node);
        kprint(i < BSP_NIC_COUNT ? " (BSP)\n" : " (AP pool)\n");
    }
    for (uint32 i = 0; i < cpu_count; i++) {
        kprint("NIC: CPU ");
        kprint_dec(i);
        kprint(" NUMA ");
        kprint_dec(thread_meta[i].numa_node);
        kprint(" -> NIC ");
        if (thread_meta[i].nic_index == NIC_NONE)
            kprint("none");
        else
            kprint_dec(thread_meta[i].nic_index);
        kprint("\n");
    }
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

uint32 nic_get_numa_node(uint32 idx) {
    if (idx >= nic_count || !nics[idx].active)
        return PCI_NUMA_UNKNOWN;
    return nics[idx].numa_node;
}

int nic_find_for_node(uint32 node) {
    for (uint32 i = 0; i < nic_count; i++) {
        if (nics[i].active && nics[i].numa_node == node)
            return (int)i;
    }
    return -1;
}

NicAssignmentMode nic_get_mode(void) {
    return current_mode;
}

void nic_set_mode(NicAssignmentMode mode) {
    current_mode = mode;
}

// Choose a sensible default mode based on the resource counts.
// - per-core if there are enough AP NICs for every AP core
// - per-numa otherwise
static NicAssignmentMode nic_choose_default_mode(void) {
    uint32 ap_nics = (nic_count > BSP_NIC_COUNT) ? (nic_count - BSP_NIC_COUNT) : 0;
    uint32 ap_cores = (cpu_count > 0) ? (cpu_count - 1) : 0;
    if (ap_cores > 0 && ap_nics >= ap_cores)
        return NIC_MODE_PER_CORE;
    return NIC_MODE_PER_NUMA;
}

// Fill a ThreadMeta entry from a NIC slot.
static void fill_thread_meta_nic(ThreadMeta *tm, uint32 nic_idx) {
    NICSlot *slot = &nics[nic_idx];
    VirtioNetDevice *vnet = (VirtioNetDevice *)slot->dev;
    const PCIDevice *pci = vnet->vdev.pci;
    tm->nic_index = nic_idx;
    tm->nic_segment = pci->segment;
    tm->nic_bus = pci->bus;
    tm->nic_dev = pci->dev;
    tm->nic_func = pci->func;
    slot->ops.get_mac(slot->dev, tm->nic_mac);
}

void nic_assign(void) {
    // Initialize all CPUs to "no NIC"
    for (uint32 i = 0; i < MAX_CPUS; i++) {
        ThreadMeta *tm = &thread_meta[i];
        tm->cpu_index = i;
        tm->numa_node = THREAD_NUMA_UNKNOWN;
        tm->nic_index = NIC_NONE;
        tm->nic_segment = 0;
        tm->nic_bus = 0;
        tm->nic_dev = 0;
        tm->nic_func = 0;
        for (int j = 0; j < 6; j++) tm->nic_mac[j] = 0;
    }

    // Populate per-CPU NUMA nodes from ACPI
    for (uint32 i = 0; i < cpu_count; i++) {
        uint32 node = 0;
        if (acpi_cpu_to_node(percpu[i].lapic_id, &node))
            thread_meta[i].numa_node = node;
    }

    if (nic_count <= BSP_NIC_COUNT) return;  // no AP NICs available

    // CPU 0 is BSP - kernel-managed, no AP NIC assignment
    if (current_mode == NIC_MODE_PER_NUMA) {
        // Each AP gets the first AP-pool NIC matching its NUMA node.
        // If no NIC on that node, leave nic_index = NIC_NONE.
        for (uint32 cpu = 1; cpu < cpu_count; cpu++) {
            uint32 node = thread_meta[cpu].numa_node;
            for (uint32 nic_idx = BSP_NIC_COUNT; nic_idx < nic_count; nic_idx++) {
                if (nics[nic_idx].active && nics[nic_idx].numa_node == node) {
                    fill_thread_meta_nic(&thread_meta[cpu], nic_idx);
                    break;
                }
            }
        }
    } else if (current_mode == NIC_MODE_PER_CORE) {
        // One NIC per AP CPU, locality-respecting.
        // Track which AP-pool NICs are still free.
        uint8 nic_used[MAX_NICS] = {0};
        for (uint32 cpu = 1; cpu < cpu_count; cpu++) {
            uint32 node = thread_meta[cpu].numa_node;
            for (uint32 nic_idx = BSP_NIC_COUNT; nic_idx < nic_count; nic_idx++) {
                if (nic_used[nic_idx]) continue;
                if (!nics[nic_idx].active) continue;
                if (nics[nic_idx].numa_node != node) continue;
                fill_thread_meta_nic(&thread_meta[cpu], nic_idx);
                nic_used[nic_idx] = 1;
                break;
            }
        }
    }
}

int nic_enable_intx(uint32 idx, uint8 vector) {
    if (idx >= MAX_NICS || !nics[idx].active) return -1;

    VirtioNetDevice *vnet = (VirtioNetDevice *)nics[idx].dev;
    if (!vnet) return -1;
    const PCIDevice *pci = vnet->vdev.pci;
    if (!pci) return -1;

    uint8 gsi = pci_find_gsi(pci);
    if (gsi == 0xFF) {
        kprint("NIC: slot ");
        kprint_dec(idx);
        kprint(" no INTx GSI available (pin=0 or unrouted)\n");
        return -1;
    }

    // Register this virtio device for the asm ISR's ack path. Today
    // we route only the mgmt NIC (slot 0) so a single registration
    // is fine. When a second consumer joins, this becomes a list.
    virtio_isr_register(&vnet->vdev);

    // Route the GSI: PCI INTx is conventionally active-low,
    // level-triggered. ISO flags 0xA = polarity 0b10 (active-low) +
    // trigger 0b10 (level). Documented in
    // ai.rules / wakelat plan: real hardware MAY indicate different
    // flags via MADT ISO entries; today we hardcode the PCI
    // convention which matches QEMU.
    ioapic_route_irq(gsi, vector, lapic_bsp_id(), 0xA);

    kprint("NIC: slot ");
    kprint_dec(idx);
    kprint(" INTx GSI ");
    kprint_dec(gsi);
    kprint(" -> vector 0x");
    if (vector >= 0x10) putc("0123456789ABCDEF"[vector >> 4]);
    putc("0123456789ABCDEF"[vector & 0xF]);
    kprint("\n");
    return 0;
}
