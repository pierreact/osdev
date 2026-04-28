#ifndef SYSTEM_PCI_H
#define SYSTEM_PCI_H

#include <types.h>

#define MAX_PCI_DEVICES 64

#define PCI_NUMA_UNKNOWN 0xFFFFFFFFu

// PCI config space offsets
#define PCI_CAP_PTR             0x34

// PCI command register bits
#define PCI_COMMAND_BUS_MASTER  0x04
#define PCI_COMMAND_MMIO        0x02
#define PCI_COMMAND_IO          0x01

typedef struct {
    uint16 vendor_id;
    uint16 device_id;
    uint8  class_code;
    uint8  subclass;
    uint8  prog_if;
    uint8  header_type;
    uint16 segment;
    uint8  bus;
    uint8  dev;
    uint8  func;
    uint64 bar[6];
    uint8  bar_is_mmio[6];
    uint8  bar_is_64bit[6];
    uint8  irq_line;      // GSI hint at PCI config 0x3C (QEMU writes the actual GSI; on real HW this is firmware's hint and may be 0xFF)
    uint8  irq_pin;       // INTx pin (1=A,2=B,3=C,4=D,0=no INTx) at PCI config 0x3D
    uint8  reserved[2];
    uint32 numa_node;     // proximity domain, or PCI_NUMA_UNKNOWN
} PCIDevice;

void            pci_init(void);
uint32          pci_get_device_count(void);
const PCIDevice *pci_get_device(uint32 index);
const PCIDevice *pci_find_device(uint16 vendor_id, uint16 device_id);
const PCIDevice *pci_find_class(uint8 class_code, uint8 subclass);
uint32          pci_config_read32(const PCIDevice *dev, uint16 offset);
void            pci_config_write32(const PCIDevice *dev, uint16 offset, uint32 val);
uint16          pci_config_read16(const PCIDevice *dev, uint16 offset);
void            pci_config_write16(const PCIDevice *dev, uint16 offset, uint16 val);
uint8           pci_config_read8(const PCIDevice *dev, uint16 offset);

void        pci_ids_init(void);    // load names from /DATA/PCI.IDS on ISO
const char *pci_vendor_name(uint16 vendor_id);     // returns NULL if unknown
const char *pci_class_name(uint8 class_code, uint8 subclass);

// Resolve the GSI for a PCI device's INTx pin.
// QEMU-specific shortcut today: returns dev->irq_line (PCI config 0x3C)
// when dev->irq_pin != 0. QEMU populates 0x3C with the actual GSI on
// q35; real hardware writes 0xFF here and expects the OS to walk
// ACPI _PRT to resolve. Caller treats 0xFF as "no IRQ available".
// Real-hardware mitigation is a followup (extend src/arch/aml.c with
// _PRT parsing) -- see ai.rules and the Phase B notes in the
// wake-up-latency plan.
uint8 pci_find_gsi(const PCIDevice *dev);

#endif
