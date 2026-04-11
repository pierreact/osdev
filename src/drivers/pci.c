#include <drivers/pci.h>
#include <arch/acpi.h>
#include <kernel/mem.h>
#include <drivers/monitor.h>

// PCI config space offsets
#define PCI_VENDOR_ID       0x00
#define PCI_DEVICE_ID       0x02
#define PCI_COMMAND         0x04
#define PCI_STATUS          0x06
#define PCI_CLASS_REVISION  0x08
#define PCI_HEADER_TYPE     0x0E
#define PCI_BAR0            0x10
#define PCI_IRQ_LINE        0x3C

static PCIDevice pci_devices[MAX_PCI_DEVICES];
static uint32 pci_count = 0;

// ECAM base addresses per segment (populated from MCFG)
static uint64 ecam_bases[MAX_PCIE_SEGMENTS];
static uint8  ecam_start_bus[MAX_PCIE_SEGMENTS];
static uint8  ecam_end_bus[MAX_PCIE_SEGMENTS];
static uint32 ecam_segment_count = 0;

// Compute ECAM MMIO address for a given BDF + offset
static volatile uint8 *ecam_addr(uint32 seg, uint8 bus, uint8 dev, uint8 func, uint16 offset) {
    uint64 addr = ecam_bases[seg]
        + ((uint64)(bus - ecam_start_bus[seg]) << 20)
        + ((uint64)dev << 15)
        + ((uint64)func << 12)
        + offset;
    return (volatile uint8 *)addr;
}

uint32 pci_config_read32(const PCIDevice *dev, uint16 offset) {
    volatile uint32 *p = (volatile uint32 *)ecam_addr(dev->segment, dev->bus, dev->dev, dev->func, offset & ~3);
    return *p;
}

void pci_config_write32(const PCIDevice *dev, uint16 offset, uint32 val) {
    volatile uint32 *p = (volatile uint32 *)ecam_addr(dev->segment, dev->bus, dev->dev, dev->func, offset & ~3);
    *p = val;
}

uint16 pci_config_read16(const PCIDevice *dev, uint16 offset) {
    volatile uint16 *p = (volatile uint16 *)ecam_addr(dev->segment, dev->bus, dev->dev, dev->func, offset & ~1);
    return *p;
}

void pci_config_write16(const PCIDevice *dev, uint16 offset, uint16 val) {
    volatile uint16 *p = (volatile uint16 *)ecam_addr(dev->segment, dev->bus, dev->dev, dev->func, offset & ~1);
    *p = val;
}

uint8 pci_config_read8(const PCIDevice *dev, uint16 offset) {
    volatile uint8 *p = ecam_addr(dev->segment, dev->bus, dev->dev, dev->func, offset);
    return *p;
}

// Decode BARs for a type 0 header (6 BARs)
static void decode_bars(PCIDevice *d) {
    for (int i = 0; i < 6; i++) {
        d->bar[i] = 0;
        d->bar_is_mmio[i] = 0;
        d->bar_is_64bit[i] = 0;
    }

    volatile uint32 *cfg_base = (volatile uint32 *)ecam_addr(0, d->bus, d->dev, d->func, 0);

    for (int i = 0; i < 6; i++) {
        uint32 bar_offset = (PCI_BAR0 + i * 4) / 4;
        uint32 orig = cfg_base[bar_offset];

        if (orig == 0)
            continue;

        // Write all 1s to determine size
        cfg_base[bar_offset] = 0xFFFFFFFF;
        uint32 mask = cfg_base[bar_offset];
        cfg_base[bar_offset] = orig;  // restore

        if (mask == 0 || mask == 0xFFFFFFFF)
            continue;

        uint8 is_io = orig & 1;
        if (is_io) {
            d->bar[i] = orig & 0xFFFFFFFC;
            d->bar_is_mmio[i] = 0;
        } else {
            uint8 type = (orig >> 1) & 3;
            d->bar_is_mmio[i] = 1;

            if (type == 0x02) {
                // 64-bit BAR
                d->bar_is_64bit[i] = 1;
                uint32 orig_hi = cfg_base[bar_offset + 1];
                d->bar[i] = ((uint64)orig_hi << 32) | (orig & 0xFFFFFFF0);
                i++;  // skip next BAR (upper 32 bits)
            } else {
                d->bar[i] = orig & 0xFFFFFFF0;
            }
        }
    }
}

static void enumerate_segment(uint32 seg_idx) {
    uint8 start = ecam_start_bus[seg_idx];
    uint8 end = ecam_end_bus[seg_idx];

    for (uint32 bus = start; bus <= end; bus++) {
        for (uint8 dev = 0; dev < 32; dev++) {
            for (uint8 func = 0; func < 8; func++) {
                if (pci_count >= MAX_PCI_DEVICES)
                    return;

                volatile uint16 *vid_ptr = (volatile uint16 *)ecam_addr(seg_idx, bus, dev, func, PCI_VENDOR_ID);
                uint16 vid = *vid_ptr;

                if (vid == 0xFFFF)
                    continue;

                volatile uint16 *did_ptr = (volatile uint16 *)ecam_addr(seg_idx, bus, dev, func, PCI_DEVICE_ID);
                volatile uint8  *hdr_ptr = ecam_addr(seg_idx, bus, dev, func, PCI_HEADER_TYPE);
                volatile uint32 *cls_ptr = (volatile uint32 *)ecam_addr(seg_idx, bus, dev, func, PCI_CLASS_REVISION);
                volatile uint8  *irq_ptr = ecam_addr(seg_idx, bus, dev, func, PCI_IRQ_LINE);

                PCIDevice *d = &pci_devices[pci_count];
                d->vendor_id = vid;
                d->device_id = *did_ptr;
                d->header_type = *hdr_ptr & 0x7F;
                uint32 class_rev = *cls_ptr;
                d->class_code = (class_rev >> 24) & 0xFF;
                d->subclass   = (class_rev >> 16) & 0xFF;
                d->prog_if    = (class_rev >> 8) & 0xFF;
                d->segment = seg_idx;
                d->bus = bus;
                d->dev = dev;
                d->func = func;
                d->irq_line = *irq_ptr;

                uint32 node = 0;
                if (acpi_pci_to_node(seg_idx, bus, dev, func, &node))
                    d->numa_node = node;
                else
                    d->numa_node = PCI_NUMA_UNKNOWN;

                if (d->header_type == 0)
                    decode_bars(d);

                pci_count++;

                // If not multi-function, skip functions 1-7
                if (func == 0 && !(*hdr_ptr & 0x80))
                    break;
            }
        }
    }
}

void pci_init(void) {
    pci_count = 0;
    ecam_segment_count = 0;

    uint32 seg_count = 0;
    const ACPIPCIEConfigSegment *segs = acpi_pcie_ecam_segments(&seg_count);

    if (seg_count == 0) {
        kprint("PCI: No ECAM segments found\n");
        return;
    }

    ecam_segment_count = seg_count;
    for (uint32 i = 0; i < seg_count && i < MAX_PCIE_SEGMENTS; i++) {
        ecam_bases[i] = segs[i].base_address;
        ecam_start_bus[i] = segs[i].start_bus;
        ecam_end_bus[i] = segs[i].end_bus;

        // Map ECAM MMIO for this segment
        uint64 bus_count = (uint64)(segs[i].end_bus - segs[i].start_bus + 1);
        uint64 size = bus_count * 32 * 8 * 4096;  // buses * devices * functions * 4KB
        map_mmio_range(segs[i].base_address, size);
    }

    // Enumerate all segments
    for (uint32 i = 0; i < ecam_segment_count; i++)
        enumerate_segment(i);

    kprint("PCI: ");
    kprint_dec(pci_count);
    kprint(" devices found\n");
}

uint32 pci_get_device_count(void) {
    return pci_count;
}

const PCIDevice *pci_get_device(uint32 index) {
    if (index >= pci_count)
        return NULL;
    return &pci_devices[index];
}

const PCIDevice *pci_find_device(uint16 vendor_id, uint16 device_id) {
    for (uint32 i = 0; i < pci_count; i++) {
        if (pci_devices[i].vendor_id == vendor_id && pci_devices[i].device_id == device_id)
            return &pci_devices[i];
    }
    return NULL;
}

const PCIDevice *pci_find_class(uint8 class_code, uint8 subclass) {
    for (uint32 i = 0; i < pci_count; i++) {
        if (pci_devices[i].class_code == class_code && pci_devices[i].subclass == subclass)
            return &pci_devices[i];
    }
    return NULL;
}
