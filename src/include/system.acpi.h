#ifndef SYSTEM_ACPI_H
#define SYSTEM_ACPI_H

#include <types.h>

#define MAX_CPUS 16
#define MAX_ISOS 16
#define MAX_ACPI_TABLES 32

// Cached info about one ACPI table found in the RSDT
typedef struct {
    char     signature[5]; // 4 chars + null
    uint32   address;      // Physical address from RSDT
    uint32   length;
    uint8    revision;
} ACPITableEntry;

extern ACPITableEntry acpi_tables[MAX_ACPI_TABLES];
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

void acpi_init(void);
void acpi_lsacpi(void);

#endif
