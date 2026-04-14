#ifndef SYSTEM_ACPI_H
#define SYSTEM_ACPI_H

#include <types.h>

#define MAX_CPUS 16
#define MAX_ISOS 16
#define MAX_ACPI_TABLES 32
#define MAX_NUMA_NODES 16
#define MAX_NUMA_CPU_AFFINITIES 32
#define MAX_NUMA_MEM_AFFINITIES 32
#define MAX_NUMA_PCI_AFFINITIES 32
#define MAX_PCIE_SEGMENTS 16
#define MAX_IOMMU_UNITS 16

// Cached info about one ACPI table found in the RSDT
typedef struct {
    char     signature[5]; // 4 chars + null
    uint64   address;      // Physical address from RSDT/XSDT
    uint32   length;
    uint8    revision;
} ACPITableEntry;

typedef enum {
    ACPI_TABLE_ABSENT = 0,
    ACPI_TABLE_PRESENT = 1,
    ACPI_TABLE_PARSED = 2,
    ACPI_TABLE_PARSE_ERROR = 3
} ACPITableStatus;

extern ACPITableEntry acpi_tables[MAX_ACPI_TABLES];
extern ACPITableStatus acpi_table_statuses[MAX_ACPI_TABLES];
extern uint32 acpi_table_count;
extern uint8  acpi_revision; // RSDP revision (0 = ACPI 1.0, 2 = ACPI 2.0+)

// Root System Description Pointer - entry point for all ACPI tables
typedef struct __attribute__((packed)) {
    char     signature[8];    // "RSD PTR "
    uint8    checksum;
    char     oem_id[6];
    uint8    revision;        // 0 = ACPI 1.0, 2 = ACPI 2.0+
    uint32   rsdt_address;
} RSDP;

// Common header for all ACPI System Description Tables
typedef struct __attribute__((packed)) {
    char     signature[4];
    uint32   length;
    uint8    revision;
    uint8    checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32   oem_revision;
    uint32   creator_id;
    uint32   creator_revision;
} ACPISDTHeader;

// Multiple APIC Description Table - lists all interrupt controllers and CPUs
typedef struct __attribute__((packed)) {
    ACPISDTHeader header;
    uint32        local_apic_addr;  // Physical address of Local APIC
    uint32        flags;
} MADT;

// MADT entry type 0: one per logical processor
typedef struct __attribute__((packed)) {
    uint8    type;            // 0
    uint8    length;
    uint8    acpi_processor_id;
    uint8    apic_id;
    uint32   flags;           // bit 0 = enabled
} MADTLocalAPIC;

// MADT entry type 1: I/O APIC
typedef struct __attribute__((packed)) {
    uint8    type;            // 1
    uint8    length;
    uint8    ioapic_id;
    uint8    reserved;
    uint32   ioapic_addr;    // MMIO base address
    uint32   gsi_base;       // First GSI this I/O APIC handles
} MADTIOAPIC;

// MADT entry type 2: maps ISA IRQ to GSI (polarity/trigger may differ)
typedef struct __attribute__((packed)) {
    uint8    type;            // 2
    uint8    length;
    uint8    bus_source;      // Always 0 (ISA)
    uint8    irq_source;      // ISA IRQ number
    uint32   gsi;             // Global System Interrupt it maps to
    uint16   flags;           // Polarity (bits 1:0), trigger (bits 3:2)
} MADTIntSrcOverride;

// Simplified ISO entry stored after MADT parsing
typedef struct {
    uint8    irq_source;
    uint32   gsi;
    uint16   flags;
} ISOEntry;

extern uint8  cpu_lapic_ids[MAX_CPUS];
extern uint32 cpu_count;
extern uint32 lapic_base_addr;
extern uint32 ioapic_base_addr;
extern ISOEntry iso_entries[MAX_ISOS];
extern uint32 iso_count;

typedef struct {
    uint8    lapic_id;
    uint32   proximity_domain;
    uint32   flags;
} ACPINumaCpuAffinity;

typedef struct {
    uint64   base;
    uint64   length;
    uint32   proximity_domain;
    uint32   flags;
} ACPINumaMemAffinity;

typedef struct {
    uint16   segment;
    uint16   bdf;             // bus<<8 | dev<<3 | func
    uint32   proximity_domain;
    uint32   flags;
} ACPINumaPCIAffinity;

typedef struct {
    uint64   base_address;
    uint16   segment_group;
    uint8    start_bus;
    uint8    end_bus;
} ACPIPCIEConfigSegment;

typedef struct {
    uint64   address;
    uint16   min_tick;
    uint8    comparator_count;
    uint8    counter_size_64;
    uint8    present;
} ACPIHPETInfo;

typedef struct {
    uint64   address;
    uint8    bit_width;
    uint8    present;
} ACPIPMTimerInfo;

typedef struct {
    uint8    space_id;
    uint8    bit_width;
    uint8    bit_offset;
    uint8    access_size;
    uint64   address;
    uint8    value;
    uint8    present;
} ACPIResetRegInfo;

typedef struct {
    char     source[5]; // DMAR/IVRS
    uint16   segment;
    uint64   register_base;
} ACPIIOMMUUnit;

void acpi_init(void);
void acpi_lsacpi(void);

int acpi_has_table(const char *sig);
ACPISDTHeader *acpi_get_table(const char *sig);
ACPITableStatus acpi_get_table_status(const char *sig);

uint32 acpi_numa_node_count(void);
int acpi_cpu_to_node(uint8 lapic_id, uint32 *node_out);
int acpi_distance(uint32 from_node, uint32 to_node, uint8 *distance_out);
const ACPINumaMemAffinity *acpi_memory_affinities(uint32 *count_out);
const ACPINumaPCIAffinity *acpi_pci_affinities(uint32 *count_out);
int acpi_pci_to_node(uint16 segment, uint8 bus, uint8 dev, uint8 func, uint32 *node_out);

const ACPIPCIEConfigSegment *acpi_pcie_ecam_segments(uint32 *count_out);
int acpi_hpet_info(ACPIHPETInfo *out);
int acpi_pm_timer_info(ACPIPMTimerInfo *out);
int acpi_reset_reg_info(ACPIResetRegInfo *out);
const ACPIIOMMUUnit *acpi_iommu_units(uint32 *count_out);

#endif
