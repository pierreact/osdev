// MADT parser: extract Local APIC, I/O APIC, and Interrupt Source
// Override entries. Populates cpu_count/cpu_lapic_ids/lapic_base_addr/
// ioapic_base_addr/iso_entries (all public, defined in acpi.c).

#include <types.h>
#include <arch/acpi.h>
#include "acpi_internal.h"

uint32 madt_type0_seen = 0;
uint32 madt_type0_usable = 0;
uint32 madt_type9_seen = 0;
uint32 madt_type9_usable = 0;

void acpi_madt_reset(void) {
    madt_type0_seen = 0;
    madt_type0_usable = 0;
    madt_type9_seen = 0;
    madt_type9_usable = 0;
}

void parse_madt(ACPISDTHeader *hdr) {
    MADT *madt = (MADT *)hdr;
    lapic_base_addr = madt->local_apic_addr;

    // Walk variable-length entries after the fixed MADT header
    uint8 *ptr = (uint8 *)madt + sizeof(MADT);
    uint8 *end = (uint8 *)madt + madt->header.length;

    while (ptr < end) {
        uint8 type = ptr[0];
        uint8 len  = ptr[1];
        if (len < 2) break;

        switch (type) {
            case 0: { // Local APIC - one per CPU
                MADTLocalAPIC *lapic = (MADTLocalAPIC *)ptr;
                madt_type0_seen++;
                // Accept enabled (bit 0) and online-capable (bit 1).
                if ((lapic->flags & 0x3) && cpu_count < MAX_CPUS) {
                    madt_type0_usable++;
                    cpu_lapic_ids[cpu_count] = lapic->apic_id;
                    cpu_count++;
                }
                break;
            }
            case 1: { // I/O APIC
                MADTIOAPIC *ioapic = (MADTIOAPIC *)ptr;
                ioapic_base_addr = ioapic->ioapic_addr;
                break;
            }
            case 2: { // Interrupt Source Override - ISA IRQ to GSI remapping
                MADTIntSrcOverride *iso = (MADTIntSrcOverride *)ptr;
                if (iso_count < MAX_ISOS) {
                    iso_entries[iso_count].irq_source = iso->irq_source;
                    iso_entries[iso_count].gsi = iso->gsi;
                    iso_entries[iso_count].flags = iso->flags;
                    iso_count++;
                }
                break;
            }
            case 9: { // Processor Local x2APIC
                // Layout: type(1), len(1), reserved(2), x2apic_id(4), flags(4), acpi_uid(4)
                if (len >= 16) {
                    madt_type9_seen++;
                    uint32 x2apic_id = *(uint32 *)(void *)(ptr + 4);
                    uint32 flags = *(uint32 *)(void *)(ptr + 8);
                    // Accept enabled (bit 0) and online-capable (bit 1).
                    if ((flags & 0x3) && cpu_count < MAX_CPUS) {
                        madt_type9_usable++;
                        cpu_lapic_ids[cpu_count] = (uint8)(x2apic_id & 0xFF);
                        cpu_count++;
                    }
                }
                break;
            }
        }
        ptr += len;
    }
}
