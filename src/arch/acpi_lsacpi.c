// sys.acpi.ls shell command: print RSDP info, every known-to-us ACPI
// table with parse status, and a short summary of extracted facts.

#include <types.h>
#include <arch/acpi.h>
#include <drivers/monitor.h>
#include "acpi_internal.h"

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
    for (uint32 i = 0; i < acpi_table_count; i++) {
        if (signatures_equal(acpi_tables[i].signature, sig))
            return (int)acpi_table_statuses[i];
    }
    return 0;
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
        kprint("  -    -                  -         [absent]  ");
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
    if (saved_root_is_xsdt)
        kprint("XSDT at ");
    else
        kprint("RSDT at ");
    kprint_long2hex(saved_rsdt_xsdt_addr, "\n\n");

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

    kprint("\nExtracted facts:\n");
    kprint("  NUMA nodes: ");
    kprint_dec(numa_node_count);
    kprint("\n");
    kprint("  NUMA memory ranges: ");
    kprint_dec(numa_mem_affinity_count);
    kprint("\n");
    kprint("  PCIe ECAM segments: ");
    kprint_dec(pcie_segment_count);
    kprint("\n");
    kprint("  IOMMU units: ");
    kprint_dec(iommu_unit_count);
    kprint("\n");
    if (hpet_info.present) {
        kprint("  HPET base: ");
        kprint_long2hex(hpet_info.address, "");
    }
    if (pm_timer_info.present) {
        kprint("  PM timer: ");
        kprint_long2hex(pm_timer_info.address, "");
    }
    if (reset_reg_info.present) {
        kprint("  ACPI reset reg: ");
        kprint_long2hex(reset_reg_info.address, "");
    }
    kprint("\n");
}
