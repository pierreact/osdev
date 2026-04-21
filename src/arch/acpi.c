// ACPI core: RSDP/RSDT/XSDT discovery, shared utilities, per-table
// dispatch, and acpi_init orchestration (including the AML bridge for
// DSDT/SSDT). Per-table parsers and accessors live in sibling
// acpi_*.c files; see acpi_internal.h for the shared contract.

#include <types.h>
#include <arch/acpi.h>
#include <arch/aml.h>
#include <drivers/monitor.h>
#include "acpi_internal.h"

// ============================================================================
// Public globals (extern in arch/acpi.h). MADT fills cpu_*, lapic_*,
// ioapic_* and iso_*; RSDP discovery fills acpi_tables, statuses,
// count, and revision.
// ============================================================================
uint8  cpu_lapic_ids[MAX_CPUS];
uint32 cpu_count = 0;
uint32 lapic_base_addr = 0;
uint32 ioapic_base_addr = 0;
ISOEntry iso_entries[MAX_ISOS];
uint32 iso_count = 0;

ACPITableEntry acpi_tables[MAX_ACPI_TABLES];
ACPITableStatus acpi_table_statuses[MAX_ACPI_TABLES];
uint32 acpi_table_count = 0;
uint8  acpi_revision = 0;

// RSDP metadata kept for acpi_lsacpi display.
RSDP   *saved_rsdp = NULL;
uint64  saved_rsdt_xsdt_addr = 0;
uint8   saved_root_is_xsdt = 0;

// ACPI 2.0+ extended RSDP layout. Private to discovery.
typedef struct __attribute__((packed)) {
    char     signature[8];
    uint8    checksum;
    char     oem_id[6];
    uint8    revision;
    uint32   rsdt_address;
    uint32   length;
    uint64   xsdt_address;
    uint8    extended_checksum;
    uint8    reserved[3];
} RSDP20;

// ============================================================================
// Shared utilities (declared in acpi_internal.h)
// ============================================================================
int memcmp_bytes(const void *a, const void *b, uint32 len) {
    const uint8 *p = (const uint8 *)a;
    const uint8 *q = (const uint8 *)b;
    for (uint32 i = 0; i < len; i++) {
        if (p[i] != q[i]) return 1;
    }
    return 0;
}

uint8 checksum_bytes(const void *ptr, uint32 len) {
    const uint8 *p = (const uint8 *)ptr;
    uint8 sum = 0;
    for (uint32 i = 0; i < len; i++) sum = (uint8)(sum + p[i]);
    return sum;
}

int signatures_equal(const char *a, const char *b) {
    return memcmp_bytes(a, b, 4) == 0;
}

int validate_sdt(ACPISDTHeader *h) {
    if (!h) return 0;
    if (h->length < sizeof(ACPISDTHeader)) return 0;
    return checksum_bytes(h, h->length) == 0;
}

// ============================================================================
// RSDP/RSDT/XSDT discovery
// ============================================================================

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

static int validate_rsdp(RSDP *rsdp) {
    if (!rsdp) return 0;
    if (memcmp_bytes(rsdp->signature, "RSD PTR ", 8) != 0) return 0;
    if (checksum_bytes(rsdp, 20) != 0) return 0;
    if (rsdp->revision >= 2) {
        RSDP20 *rsdp20 = (RSDP20 *)rsdp;
        if (rsdp20->length < sizeof(RSDP20)) return 0;
        if (checksum_bytes(rsdp20, rsdp20->length) != 0) return 0;
    }
    return 1;
}

static void enumerate_root_table_entries(uint64 root_addr, uint8 entry_size) {
    acpi_table_count = 0;
    ACPISDTHeader *root = (ACPISDTHeader *)root_addr;
    // Be tolerant here: some firmware has bad root checksum but valid entries.
    if (!root || root->length < sizeof(ACPISDTHeader)) {
        return;
    }

    uint32 entries = (root->length - sizeof(ACPISDTHeader)) / entry_size;
    uint8 *table_ptrs = (uint8 *)((uint64)root + sizeof(ACPISDTHeader));
    for (uint32 i = 0; i < entries && acpi_table_count < MAX_ACPI_TABLES; i++) {
        uint64 table_addr;
        if (entry_size == 8)
            table_addr = ((uint64 *)table_ptrs)[i];
        else
            table_addr = (uint64)((uint32 *)table_ptrs)[i];

        ACPISDTHeader *h = (ACPISDTHeader *)table_addr;
        if (!h || h->length < sizeof(ACPISDTHeader)) continue;

        ACPITableEntry *e = &acpi_tables[acpi_table_count];
        e->signature[0] = h->signature[0];
        e->signature[1] = h->signature[1];
        e->signature[2] = h->signature[2];
        e->signature[3] = h->signature[3];
        e->signature[4] = '\0';
        e->address = table_addr;
        e->length = h->length;
        e->revision = h->revision;
        acpi_table_statuses[acpi_table_count] = ACPI_TABLE_PRESENT;
        acpi_table_count++;
    }
}

// Walk RSDT/XSDT entries and record each table's signature, address, length
static void enumerate_root_tables(RSDP *rsdp) {
    saved_root_is_xsdt = 0;
    saved_rsdt_xsdt_addr = 0;

    // Prefer XSDT when available, but fall back to RSDT if APIC is missing.
    if (rsdp->revision >= 2) {
        RSDP20 *rsdp20 = (RSDP20 *)rsdp;
        if (rsdp20->xsdt_address) {
            saved_root_is_xsdt = 1;
            saved_rsdt_xsdt_addr = rsdp20->xsdt_address;
            enumerate_root_table_entries(rsdp20->xsdt_address, 8);
            if (acpi_has_table("APIC")) return;
        }
    }

    saved_root_is_xsdt = 0;
    saved_rsdt_xsdt_addr = rsdp->rsdt_address;
    enumerate_root_table_entries(rsdp->rsdt_address, 4);
}

// ============================================================================
// Table accessors (public API in arch/acpi.h)
// ============================================================================

int acpi_has_table(const char *sig) {
    for (uint32 i = 0; i < acpi_table_count; i++) {
        if (signatures_equal(acpi_tables[i].signature, sig)) return 1;
    }
    return 0;
}

ACPISDTHeader *acpi_get_table(const char *sig) {
    for (uint32 i = 0; i < acpi_table_count; i++) {
        if (signatures_equal(acpi_tables[i].signature, sig))
            return (ACPISDTHeader *)acpi_tables[i].address;
    }
    return NULL;
}

ACPITableStatus acpi_get_table_status(const char *sig) {
    for (uint32 i = 0; i < acpi_table_count; i++) {
        if (signatures_equal(acpi_tables[i].signature, sig))
            return acpi_table_statuses[i];
    }
    return ACPI_TABLE_ABSENT;
}

// ============================================================================
// Parser dispatch
// ============================================================================

typedef void (*TableParserFn)(ACPISDTHeader *hdr);
typedef struct {
    char sig[5];
    TableParserFn fn;
} TableParser;

static TableParser table_parsers[] = {
    { "APIC", parse_madt },
    { "SRAT", parse_srat },
    { "SLIT", parse_slit },
    { "HPET", parse_hpet },
    { "FACP", parse_fadt },
    { "MCFG", parse_mcfg },
    { "DMAR", parse_dmar },
    { "IVRS", parse_ivrs },
    { "", NULL }
};

static void reset_parsed_state(void) {
    // MADT-owned public globals defined in acpi.c
    cpu_count = 0;
    lapic_base_addr = 0;
    ioapic_base_addr = 0;
    iso_count = 0;
    // Per-module internal state
    acpi_madt_reset();
    acpi_numa_reset();
    acpi_platform_reset();
}

static void dispatch_parsers(void) {
    for (uint32 i = 0; i < acpi_table_count; i++) {
        ACPISDTHeader *h = (ACPISDTHeader *)acpi_tables[i].address;
        ACPITableStatus status = ACPI_TABLE_PRESENT;
        int valid = validate_sdt(h);
        for (uint32 p = 0; table_parsers[p].sig[0]; p++) {
            if (signatures_equal(acpi_tables[i].signature, table_parsers[p].sig)) {
                if (!valid) {
                    status = ACPI_TABLE_PARSE_ERROR;
                    // Keep APIC parsing best-effort even with bad checksum so IRQ routing still works.
                    if (signatures_equal(acpi_tables[i].signature, "APIC"))
                        table_parsers[p].fn(h);
                } else {
                    table_parsers[p].fn(h);
                    status = ACPI_TABLE_PARSED;
                }
                break;
            }
        }
        acpi_table_statuses[i] = status;
    }
}

// ============================================================================
// Orchestration
// ============================================================================

// Find RSDP, enumerate RSDT, parse MADT for CPU and interrupt controller info
void acpi_init(void) {
    RSDP *rsdp = find_rsdp();
    if (!validate_rsdp(rsdp)) {
        kprint("ACPI: RSDP not found!\n");
        return;
    }
    saved_rsdp = rsdp;
    acpi_revision = rsdp->revision;
    reset_parsed_state();

    enumerate_root_tables(rsdp);

    kprint("ACPI: ");
    kprint_dec(acpi_table_count);
    kprint(" tables found (rev ");
    kprint_dec(acpi_revision);
    kprint(")\n");

    kprint("ACPI: MADT entries T0 ");
    kprint_dec(madt_type0_seen);
    kprint("/");
    kprint_dec(madt_type0_usable);
    kprint(", T9 ");
    kprint_dec(madt_type9_seen);
    kprint("/");
    kprint_dec(madt_type9_usable);
    kprint("\n");

    dispatch_parsers();

    // Parse DSDT and any SSDTs for AML _BBN/_PXM (PCI host bridge proximity).
    // DSDT is referenced by FADT (x_dsdt or dsdt). SSDTs appear directly in
    // the root table.
    ACPISDTHeader *facp = acpi_get_table("FACP");
    if (facp) {
        FADT *fadt = (FADT *)facp;
        uint64 dsdt_addr = fadt->x_dsdt ? fadt->x_dsdt : (uint64)fadt->dsdt;
        if (dsdt_addr) {
            ACPISDTHeader *dsdt = (ACPISDTHeader *)dsdt_addr;
            if (validate_sdt(dsdt) &&
                signatures_equal(dsdt->signature, "DSDT")) {
                aml_parse((uint8 *)dsdt + sizeof(ACPISDTHeader),
                          dsdt->length - sizeof(ACPISDTHeader));
            }
        }
    }
    for (uint32 i = 0; i < acpi_table_count; i++) {
        if (signatures_equal(acpi_tables[i].signature, "SSDT")) {
            ACPISDTHeader *ssdt = (ACPISDTHeader *)acpi_tables[i].address;
            if (validate_sdt(ssdt)) {
                aml_parse((uint8 *)ssdt + sizeof(ACPISDTHeader),
                          ssdt->length - sizeof(ACPISDTHeader));
            }
        }
    }

    // Keep interrupt routing functional even when MADT is absent or malformed.
    if (lapic_base_addr == 0) lapic_base_addr = 0xFEE00000;
    if (ioapic_base_addr == 0) ioapic_base_addr = 0xFEC00000;
    if (cpu_count == 0) {
        cpu_count = 1;
        cpu_lapic_ids[0] = 0;
    }

    kprint("ACPI: ");
    kprint_dec(cpu_count);
    kprint(" CPUs, LAPIC at ");
    kprint_long2hex(lapic_base_addr, "");
    kprint("IOAPIC at ");
    kprint_long2hex(ioapic_base_addr, "\n");
}
