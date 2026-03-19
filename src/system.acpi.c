#include <types.h>
#include <system.acpi.h>
#include <system.monitor.h>

uint8  cpu_lapic_ids[MAX_CPUS];
uint32 cpu_count = 0;
uint32 lapic_base_addr = 0;
uint32 ioapic_base_addr = 0;
ISOEntry iso_entries[MAX_ISOS];
uint32 iso_count = 0;

ACPITableEntry acpi_tables[MAX_ACPI_TABLES];
uint32 acpi_table_count = 0;
uint8  acpi_revision = 0;

static RSDP *saved_rsdp = NULL;

static int memcmp_bytes(const void *a, const void *b, uint32 len) {
    const uint8 *p = (const uint8 *)a;
    const uint8 *q = (const uint8 *)b;
    for (uint32 i = 0; i < len; i++) {
        if (p[i] != q[i]) return 1;
    }
    return 0;
}

// Scan for RSDP signature "RSD PTR " in EBDA and BIOS ROM area
static RSDP *find_rsdp(void) {
    // Search EBDA (pointer at 0x040E, segment address)
    uint16 ebda_seg = *(volatile uint16 *)0x040E;
    uint64 ebda_addr = (uint64)ebda_seg << 4;
    if (ebda_addr) {
        for (uint64 addr = ebda_addr; addr < ebda_addr + 1024; addr += 16) {
            if (memcmp_bytes((void *)addr, "RSD PTR ", 8) == 0) {
                return (RSDP *)addr;
            }
        }
    }

    // Search 0xE0000 - 0xFFFFF
    for (uint64 addr = 0xE0000; addr < 0x100000; addr += 16) {
        if (memcmp_bytes((void *)addr, "RSD PTR ", 8) == 0) {
            return (RSDP *)addr;
        }
    }

    return NULL;
}

// Walk RSDT entries and record each table's signature, address, length
static void enumerate_rsdt(RSDP *rsdp) {
    ACPISDTHeader *rsdt = (ACPISDTHeader *)(uint64)rsdp->rsdt_address;
    uint32 entries = (rsdt->length - sizeof(ACPISDTHeader)) / 4; // RSDT entries are 32-bit pointers
    uint32 *table_ptrs = (uint32 *)((uint64)rsdt + sizeof(ACPISDTHeader));

    acpi_table_count = 0;
    for (uint32 i = 0; i < entries && acpi_table_count < MAX_ACPI_TABLES; i++) {
        ACPISDTHeader *h = (ACPISDTHeader *)(uint64)table_ptrs[i];
        ACPITableEntry *e = &acpi_tables[acpi_table_count];
        e->signature[0] = h->signature[0];
        e->signature[1] = h->signature[1];
        e->signature[2] = h->signature[2];
        e->signature[3] = h->signature[3];
        e->signature[4] = '\0';
        e->address = table_ptrs[i];
        e->length = h->length;
        e->revision = h->revision;
        acpi_table_count++;
    }
}

// Look up an ACPI table by its 4-byte signature
static ACPISDTHeader *find_table(const char *sig) {
    for (uint32 i = 0; i < acpi_table_count; i++) {
        if (memcmp_bytes(acpi_tables[i].signature, sig, 4) == 0) {
            return (ACPISDTHeader *)(uint64)acpi_tables[i].address;
        }
    }
    return NULL;
}

// Parse MADT: extract Local APIC, I/O APIC, and Interrupt Source Override entries
static void parse_madt(MADT *madt) {
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
                if ((lapic->flags & 1) && cpu_count < MAX_CPUS) { // bit 0 = processor enabled
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
        }
        ptr += len;
    }
}

// Find RSDP, enumerate RSDT, parse MADT for CPU and interrupt controller info
void acpi_init(void) {
    RSDP *rsdp = find_rsdp();
    if (!rsdp) {
        kprint("ACPI: RSDP not found!\n");
        return;
    }
    saved_rsdp = rsdp;
    acpi_revision = rsdp->revision;

    enumerate_rsdt(rsdp);

    kprint("ACPI: ");
    kprint_dec(acpi_table_count);
    kprint(" tables found (rev ");
    kprint_dec(acpi_revision);
    kprint(")\n");

    MADT *madt = (MADT *)find_table("APIC");
    if (!madt) {
        kprint("ACPI: MADT not found!\n");
        return;
    }

    parse_madt(madt);

    kprint("ACPI: ");
    kprint_dec(cpu_count);
    kprint(" CPUs, LAPIC at ");
    kprint_long2hex(lapic_base_addr, "");
    kprint("IOAPIC at ");
    kprint_long2hex(ioapic_base_addr, "\n");
}

typedef struct {
    char sig[5];
    char *description;
} KnownTable;

static KnownTable known_tables[] = {
    { "APIC", "Multiple APIC Description Table" },
    { "FACP", "Fixed ACPI Description Table" },
    { "DSDT", "Differentiated System Description Table" },
    { "SSDT", "Secondary System Description Table" },
    { "SRAT", "System Resource Affinity Table" },
    { "SLIT", "System Locality Information Table" },
    { "HPET", "High Precision Event Timer" },
    { "MCFG", "PCI Express Memory Mapped Configuration" },
    { "DMAR", "DMA Remapping Table" },
    { "WAET", "Windows ACPI Emulated Devices Table" },
    { "HEST", "Hardware Error Source Table" },
    { "BERT", "Boot Error Record Table" },
    { "EINJ", "Error Injection Table" },
    { "ERST", "Error Record Serialization Table" },
    { "BGRT", "Boot Graphics Resource Table" },
    { "IVRS", "I/O Virtualization Reporting Structure" },
    { "DRTM", "Dynamic Root of Trust for Measurement" },
    { "TPM2", "Trusted Platform Module 2.0" },
    { "",     "" }
};

static char *table_description(const char *sig) {
    for (uint32 i = 0; known_tables[i].sig[0]; i++) {
        if (memcmp_bytes(sig, known_tables[i].sig, 4) == 0)
            return known_tables[i].description;
    }
    return "Unknown";
}

// 0 = absent, 1 = present but not parsed, 2 = parsed
static int table_status(const char *sig) {
    // Check if present in RSDT
    int present = 0;
    for (uint32 i = 0; i < acpi_table_count; i++) {
        if (memcmp_bytes(acpi_tables[i].signature, sig, 4) == 0) {
            present = 1;
            break;
        }
    }
    if (!present) return 0;
    if (memcmp_bytes(sig, "APIC", 4) == 0) return 2;
    return 1;
}

static void print_table_row(const char *sig, ACPITableEntry *e) {
    kprint((char *)sig);
    int status = table_status(sig);
    if (status == 2) {
        kprint("  ");
        kprint_dec(e->revision);
        kprint("    ");
        kprint_long2hex(e->address, "");
        kprint_dec_pad(e->length, 6);
        kprint("    [parsed]  ");
    } else if (status == 1) {
        kprint("  ");
        kprint_dec(e->revision);
        kprint("    ");
        kprint_long2hex(e->address, "");
        kprint_dec_pad(e->length, 6);
        kprint("    [present] ");
    } else {
        kprint("  -    -                  -         -    [absent]  ");
    }
    kprint(table_description(sig));
    kprint("\n");
}

// Shell command: print all known ACPI tables with status (parsed/present/absent)
void acpi_lsacpi(void) {
    if (!saved_rsdp) {
        kprint("ACPI not available\n");
        return;
    }

    kprint("ACPI revision: ");
    kprint_dec(acpi_revision);
    if (acpi_revision == 0)
        kprint(" (ACPI 1.0)\n");
    else
        kprint(" (ACPI 2.0+)\n");

    kprint("RSDP at ");
    kprint_long2hex((uint64)saved_rsdp, "");
    kprint("RSDT at ");
    kprint_long2hex((uint64)saved_rsdp->rsdt_address, "\n\n");

    kprint("SIG   REV  ADDRESS              SIZE    STATUS    DESCRIPTION\n");

    // Print all known tables in order
    for (uint32 k = 0; known_tables[k].sig[0]; k++) {
        // Find entry if present
        ACPITableEntry *e = NULL;
        for (uint32 i = 0; i < acpi_table_count; i++) {
            if (memcmp_bytes(acpi_tables[i].signature, known_tables[k].sig, 4) == 0) {
                e = &acpi_tables[i];
                break;
            }
        }
        print_table_row(known_tables[k].sig, e);
    }

    // Print any RSDT entries not in known list
    for (uint32 i = 0; i < acpi_table_count; i++) {
        int known = 0;
        for (uint32 k = 0; known_tables[k].sig[0]; k++) {
            if (memcmp_bytes(acpi_tables[i].signature, known_tables[k].sig, 4) == 0) {
                known = 1;
                break;
            }
        }
        if (!known) {
            print_table_row(acpi_tables[i].signature, &acpi_tables[i]);
        }
    }
}
