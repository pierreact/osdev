#include <types.h>
#include <arch/acpi.h>
#include <arch/aml.h>
#include <drivers/monitor.h>

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

static RSDP *saved_rsdp = NULL;
static uint64 saved_rsdt_xsdt_addr = 0;
static uint8  saved_root_is_xsdt = 0;

static ACPINumaCpuAffinity numa_cpu_affinities[MAX_NUMA_CPU_AFFINITIES];
static uint32 numa_cpu_affinity_count = 0;
static ACPINumaMemAffinity numa_mem_affinities[MAX_NUMA_MEM_AFFINITIES];
static uint32 numa_mem_affinity_count = 0;
static ACPINumaPCIAffinity numa_pci_affinities[MAX_NUMA_PCI_AFFINITIES];
static uint32 numa_pci_affinity_count = 0;
static uint32 numa_nodes[MAX_NUMA_NODES];
static uint32 numa_node_count = 0;
static uint8 slit_distances[MAX_NUMA_NODES * MAX_NUMA_NODES];
static uint8 slit_present = 0;

static ACPIPCIEConfigSegment pcie_segments[MAX_PCIE_SEGMENTS];
static uint32 pcie_segment_count = 0;

static ACPIHPETInfo hpet_info = {0};
static ACPIPMTimerInfo pm_timer_info = {0};
static ACPIResetRegInfo reset_reg_info = {0};

static ACPIIOMMUUnit iommu_units[MAX_IOMMU_UNITS];
static uint32 iommu_unit_count = 0;
static uint32 madt_type0_seen = 0;
static uint32 madt_type0_usable = 0;
static uint32 madt_type9_seen = 0;
static uint32 madt_type9_usable = 0;

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

typedef struct __attribute__((packed)) {
    uint8    space_id;
    uint8    bit_width;
    uint8    bit_offset;
    uint8    access_size;
    uint64   address;
} ACPIGAS;

typedef struct __attribute__((packed)) {
    ACPISDTHeader header;
    uint32        firmware_ctrl;
    uint32        dsdt;
    uint8         reserved0;
    uint8         preferred_pm_profile;
    uint16        sci_int;
    uint32        smi_cmd;
    uint8         acpi_enable;
    uint8         acpi_disable;
    uint8         s4bios_req;
    uint8         pstate_cnt;
    uint32        pm1a_evt_blk;
    uint32        pm1b_evt_blk;
    uint32        pm1a_cnt_blk;
    uint32        pm1b_cnt_blk;
    uint32        pm2_cnt_blk;
    uint32        pm_tmr_blk;
    uint32        gpe0_blk;
    uint32        gpe1_blk;
    uint8         pm1_evt_len;
    uint8         pm1_cnt_len;
    uint8         pm2_cnt_len;
    uint8         pm_tmr_len;
    uint8         gpe0_blk_len;
    uint8         gpe1_blk_len;
    uint8         gpe1_base;
    uint8         cst_cnt;
    uint16        p_lvl2_lat;
    uint16        p_lvl3_lat;
    uint16        flush_size;
    uint16        flush_stride;
    uint8         duty_offset;
    uint8         duty_width;
    uint8         day_alrm;
    uint8         mon_alrm;
    uint8         century;
    uint16        iapc_boot_arch;
    uint8         reserved1;
    uint32        flags;
    ACPIGAS       reset_reg;
    uint8         reset_value;
    uint8         reserved2[3];
    uint64        x_firmware_ctrl;
    uint64        x_dsdt;
    ACPIGAS       x_pm1a_evt_blk;
    ACPIGAS       x_pm1b_evt_blk;
    ACPIGAS       x_pm1a_cnt_blk;
    ACPIGAS       x_pm1b_cnt_blk;
    ACPIGAS       x_pm2_cnt_blk;
    ACPIGAS       x_pm_tmr_blk;
} FADT;

static int memcmp_bytes(const void *a, const void *b, uint32 len) {
    const uint8 *p = (const uint8 *)a;
    const uint8 *q = (const uint8 *)b;
    for (uint32 i = 0; i < len; i++) {
        if (p[i] != q[i]) return 1;
    }
    return 0;
}

static uint8 checksum_bytes(const void *ptr, uint32 len) {
    const uint8 *p = (const uint8 *)ptr;
    uint8 sum = 0;
    for (uint32 i = 0; i < len; i++) sum = (uint8)(sum + p[i]);
    return sum;
}

static int signatures_equal(const char *a, const char *b) {
    return memcmp_bytes(a, b, 4) == 0;
}

static int append_numa_node(uint32 node_id) {
    for (uint32 i = 0; i < numa_node_count; i++) {
        if (numa_nodes[i] == node_id) return 0;
    }
    if (numa_node_count >= MAX_NUMA_NODES) return 1;
    numa_nodes[numa_node_count++] = node_id;
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

static int validate_sdt(ACPISDTHeader *h) {
    if (!h) return 0;
    if (h->length < sizeof(ACPISDTHeader)) return 0;
    return checksum_bytes(h, h->length) == 0;
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

static void reset_parsed_state(void) {
    cpu_count = 0;
    lapic_base_addr = 0;
    ioapic_base_addr = 0;
    iso_count = 0;

    numa_cpu_affinity_count = 0;
    numa_mem_affinity_count = 0;
    numa_node_count = 0;
    slit_present = 0;

    pcie_segment_count = 0;
    hpet_info.present = 0;
    pm_timer_info.present = 0;
    reset_reg_info.present = 0;
    iommu_unit_count = 0;
    madt_type0_seen = 0;
    madt_type0_usable = 0;
    madt_type9_seen = 0;
    madt_type9_usable = 0;
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

static void parse_srat(ACPISDTHeader *hdr) {
    uint8 *ptr = (uint8 *)hdr + sizeof(ACPISDTHeader) + 12;
    uint8 *end = (uint8 *)hdr + hdr->length;
    while (ptr + 2 <= end) {
        uint8 type = ptr[0];
        uint8 len = ptr[1];
        if (len < 2 || ptr + len > end) break;

        if (type == 0 && len >= 16) {
            uint32 prox = (uint32)ptr[2] | ((uint32)ptr[9] << 8) | ((uint32)ptr[10] << 16) | ((uint32)ptr[11] << 24);
            uint8 apic_id = ptr[3];
            uint32 flags = *(uint32 *)(void *)(ptr + 4);
            if ((flags & 1) && numa_cpu_affinity_count < MAX_NUMA_CPU_AFFINITIES) {
                numa_cpu_affinities[numa_cpu_affinity_count].lapic_id = apic_id;
                numa_cpu_affinities[numa_cpu_affinity_count].proximity_domain = prox;
                numa_cpu_affinities[numa_cpu_affinity_count].flags = flags;
                numa_cpu_affinity_count++;
                append_numa_node(prox);
            }
        } else if (type == 1 && len >= 40) {
            uint32 prox = *(uint32 *)(void *)(ptr + 2);
            uint64 base = ((uint64)*(uint32 *)(void *)(ptr + 12) << 32) | *(uint32 *)(void *)(ptr + 8);
            uint64 length = ((uint64)*(uint32 *)(void *)(ptr + 20) << 32) | *(uint32 *)(void *)(ptr + 16);
            uint32 flags = *(uint32 *)(void *)(ptr + 28);
            if ((flags & 1) && numa_mem_affinity_count < MAX_NUMA_MEM_AFFINITIES) {
                numa_mem_affinities[numa_mem_affinity_count].base = base;
                numa_mem_affinities[numa_mem_affinity_count].length = length;
                numa_mem_affinities[numa_mem_affinity_count].proximity_domain = prox;
                numa_mem_affinities[numa_mem_affinity_count].flags = flags;
                numa_mem_affinity_count++;
                append_numa_node(prox);
            }
        } else if (type == 2 && len >= 24) {
            uint32 prox = *(uint32 *)(void *)(ptr + 4);
            uint32 x2apic_id = *(uint32 *)(void *)(ptr + 8);
            uint32 flags = *(uint32 *)(void *)(ptr + 12);
            if ((flags & 1) && numa_cpu_affinity_count < MAX_NUMA_CPU_AFFINITIES) {
                numa_cpu_affinities[numa_cpu_affinity_count].lapic_id = (uint8)x2apic_id;
                numa_cpu_affinities[numa_cpu_affinity_count].proximity_domain = prox;
                numa_cpu_affinities[numa_cpu_affinity_count].flags = flags;
                numa_cpu_affinity_count++;
                append_numa_node(prox);
            }
        } else if (type == 5 && len >= 32) {
            // Generic Initiator Affinity (ACPI 6.3+)
            // ptr[0]=type, ptr[1]=length, ptr[2]=reserved, ptr[3]=device handle type
            // ptr[4..7]=proximity domain, ptr[8..23]=device handle, ptr[24..27]=flags
            uint8 dev_handle_type = ptr[3];
            uint32 prox = *(uint32 *)(void *)(ptr + 4);
            uint32 flags = *(uint32 *)(void *)(ptr + 24);
            if ((flags & 1) && dev_handle_type == 1 &&
                numa_pci_affinity_count < MAX_NUMA_PCI_AFFINITIES) {
                // PCI device handle: 2-byte segment + 2-byte BDF, rest reserved
                uint16 segment = *(uint16 *)(void *)(ptr + 8);
                uint16 bdf     = *(uint16 *)(void *)(ptr + 10);
                numa_pci_affinities[numa_pci_affinity_count].segment = segment;
                numa_pci_affinities[numa_pci_affinity_count].bdf = bdf;
                numa_pci_affinities[numa_pci_affinity_count].proximity_domain = prox;
                numa_pci_affinities[numa_pci_affinity_count].flags = flags;
                numa_pci_affinity_count++;
                append_numa_node(prox);
            }
        }

        ptr += len;
    }
}

static void parse_slit(ACPISDTHeader *hdr) {
    if (hdr->length < sizeof(ACPISDTHeader) + 8) return;
    uint8 *ptr = (uint8 *)hdr + sizeof(ACPISDTHeader);
    uint64 locality_count = *(uint64 *)(void *)ptr;
    if (locality_count == 0 || locality_count > MAX_NUMA_NODES) return;
    uint64 expected = sizeof(ACPISDTHeader) + 8 + (locality_count * locality_count);
    if ((uint64)hdr->length < expected) return;

    uint8 *matrix = ptr + 8;
    for (uint32 i = 0; i < (uint32)locality_count; i++) {
        for (uint32 j = 0; j < (uint32)locality_count; j++) {
            slit_distances[i * MAX_NUMA_NODES + j] = matrix[i * (uint32)locality_count + j];
        }
    }
    slit_present = 1;
}

static void parse_hpet(ACPISDTHeader *hdr) {
    if (hdr->length < sizeof(ACPISDTHeader) + 20) return;
    uint8 *ptr = (uint8 *)hdr + sizeof(ACPISDTHeader);
    uint32 event_timer_block_id = *(uint32 *)(void *)ptr;
    ACPIGAS *gas = (ACPIGAS *)(void *)(ptr + 4);
    uint16 min_tick = *(uint16 *)(void *)(ptr + 16);

    hpet_info.address = gas->address;
    hpet_info.min_tick = min_tick;
    hpet_info.comparator_count = (uint8)(((event_timer_block_id >> 8) & 0x1F) + 1);
    hpet_info.counter_size_64 = (uint8)((event_timer_block_id >> 13) & 1);
    hpet_info.present = 1;
}

static void parse_fadt(ACPISDTHeader *hdr) {
    if (hdr->length < sizeof(FADT)) return;
    FADT *fadt = (FADT *)hdr;

    if (fadt->x_pm_tmr_blk.address) {
        pm_timer_info.address = fadt->x_pm_tmr_blk.address;
        pm_timer_info.bit_width = fadt->x_pm_tmr_blk.bit_width;
        pm_timer_info.present = 1;
    } else if (fadt->pm_tmr_blk) {
        pm_timer_info.address = fadt->pm_tmr_blk;
        pm_timer_info.bit_width = 32;
        pm_timer_info.present = 1;
    }

    if (fadt->reset_reg.address) {
        reset_reg_info.space_id = fadt->reset_reg.space_id;
        reset_reg_info.bit_width = fadt->reset_reg.bit_width;
        reset_reg_info.bit_offset = fadt->reset_reg.bit_offset;
        reset_reg_info.access_size = fadt->reset_reg.access_size;
        reset_reg_info.address = fadt->reset_reg.address;
        reset_reg_info.value = fadt->reset_value;
        reset_reg_info.present = 1;
    }
}

static void parse_mcfg(ACPISDTHeader *hdr) {
    if (hdr->length < sizeof(ACPISDTHeader) + 8) return;
    uint8 *ptr = (uint8 *)hdr + sizeof(ACPISDTHeader) + 8;
    uint8 *end = (uint8 *)hdr + hdr->length;
    while (ptr + 16 <= end && pcie_segment_count < MAX_PCIE_SEGMENTS) {
        pcie_segments[pcie_segment_count].base_address = *(uint64 *)(void *)(ptr + 0);
        pcie_segments[pcie_segment_count].segment_group = *(uint16 *)(void *)(ptr + 8);
        pcie_segments[pcie_segment_count].start_bus = *(uint8 *)(void *)(ptr + 10);
        pcie_segments[pcie_segment_count].end_bus = *(uint8 *)(void *)(ptr + 11);
        pcie_segment_count++;
        ptr += 16;
    }
}

static void parse_dmar(ACPISDTHeader *hdr) {
    if (hdr->length < sizeof(ACPISDTHeader) + 12) return;
    uint8 *ptr = (uint8 *)hdr + sizeof(ACPISDTHeader) + 12;
    uint8 *end = (uint8 *)hdr + hdr->length;
    while (ptr + 4 <= end && iommu_unit_count < MAX_IOMMU_UNITS) {
        uint16 type = *(uint16 *)(void *)(ptr + 0);
        uint16 len = *(uint16 *)(void *)(ptr + 2);
        if (len < 4 || ptr + len > end) break;

        if (type == 0 && len >= 16) {
            iommu_units[iommu_unit_count].source[0] = 'D';
            iommu_units[iommu_unit_count].source[1] = 'M';
            iommu_units[iommu_unit_count].source[2] = 'A';
            iommu_units[iommu_unit_count].source[3] = 'R';
            iommu_units[iommu_unit_count].source[4] = '\0';
            iommu_units[iommu_unit_count].segment = *(uint16 *)(void *)(ptr + 6);
            iommu_units[iommu_unit_count].register_base = *(uint64 *)(void *)(ptr + 8);
            iommu_unit_count++;
        }
        ptr += len;
    }
}

static void parse_ivrs(ACPISDTHeader *hdr) {
    if (hdr->length < sizeof(ACPISDTHeader) + 8) return;
    uint8 *ptr = (uint8 *)hdr + sizeof(ACPISDTHeader) + 8;
    uint8 *end = (uint8 *)hdr + hdr->length;
    while (ptr + 4 <= end && iommu_unit_count < MAX_IOMMU_UNITS) {
        uint8 type = ptr[0];
        uint16 len = *(uint16 *)(void *)(ptr + 2);
        if (len < 4 || ptr + len > end) break;

        if (type == 0x10 || type == 0x11 || type == 0x40) {
            iommu_units[iommu_unit_count].source[0] = 'I';
            iommu_units[iommu_unit_count].source[1] = 'V';
            iommu_units[iommu_unit_count].source[2] = 'R';
            iommu_units[iommu_unit_count].source[3] = 'S';
            iommu_units[iommu_unit_count].source[4] = '\0';
            iommu_units[iommu_unit_count].segment = 0xFFFF;
            iommu_units[iommu_unit_count].register_base = (len >= 16) ? *(uint64 *)(void *)(ptr + 8) : 0;
            iommu_unit_count++;
        }
        ptr += len;
    }
}

typedef void (*TableParserFn)(ACPISDTHeader *hdr);
typedef struct {
    char sig[5];
    TableParserFn fn;
} TableParser;

static TableParser table_parsers[] = {
    { "APIC", (TableParserFn)parse_madt },
    { "SRAT", parse_srat },
    { "SLIT", parse_slit },
    { "HPET", parse_hpet },
    { "FACP", parse_fadt },
    { "MCFG", parse_mcfg },
    { "DMAR", parse_dmar },
    { "IVRS", parse_ivrs },
    { "", NULL }
};

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

uint32 acpi_numa_node_count(void) {
    return numa_node_count;
}

int acpi_cpu_to_node(uint8 lapic_id, uint32 *node_out) {
    if (!node_out) return 0;
    for (uint32 i = 0; i < numa_cpu_affinity_count; i++) {
        if (numa_cpu_affinities[i].lapic_id == lapic_id) {
            *node_out = numa_cpu_affinities[i].proximity_domain;
            return 1;
        }
    }
    return 0;
}

int acpi_distance(uint32 from_node, uint32 to_node, uint8 *distance_out) {
    if (!distance_out) return 0;
    if (!slit_present) return 0;
    if (from_node >= MAX_NUMA_NODES || to_node >= MAX_NUMA_NODES) return 0;
    *distance_out = slit_distances[from_node * MAX_NUMA_NODES + to_node];
    return 1;
}

const ACPINumaMemAffinity *acpi_memory_affinities(uint32 *count_out) {
    if (count_out) *count_out = numa_mem_affinity_count;
    return numa_mem_affinities;
}

const ACPINumaPCIAffinity *acpi_pci_affinities(uint32 *count_out) {
    if (count_out) *count_out = numa_pci_affinity_count;
    return numa_pci_affinities;
}

int acpi_pci_to_node(uint16 segment, uint8 bus, uint8 dev, uint8 func, uint32 *node_out) {
    if (!node_out) return 0;

    // First, look for an exact match in SRAT Type 5 entries
    uint16 bdf = ((uint16)bus << 8) | ((uint16)(dev & 0x1F) << 3) | (func & 0x7);
    for (uint32 i = 0; i < numa_pci_affinity_count; i++) {
        if (numa_pci_affinities[i].segment == segment &&
            numa_pci_affinities[i].bdf == bdf) {
            *node_out = numa_pci_affinities[i].proximity_domain;
            return 1;
        }
    }

    // Second: try the AML walker which extracted _BBN/_PXM from DSDT/SSDT.
    // Walk up the bus number range to find the host bridge that owns this bus.
    // We pick the host bridge with the largest _BBN <= bus.
    uint32 best_node = 0;
    int best_match = -1;
    uint8 best_bbn = 0;
    uint32 hb_count = aml_host_bridge_count();
    for (uint32 i = 0; i < hb_count; i++) {
        const AMLHostBridge *hb = aml_host_bridge(i);
        if (hb->has_bbn && hb->has_pxm && hb->bus_base <= bus) {
            if (best_match < 0 || hb->bus_base > best_bbn) {
                best_bbn = hb->bus_base;
                best_node = hb->proximity;
                best_match = (int)i;
            }
        }
    }
    if (best_match >= 0) {
        *node_out = best_node;
        return 1;
    }

    // Fallback: look up the segment's ECAM base address in memory affinities
    if (segment < pcie_segment_count) {
        uint64 ecam_base = pcie_segments[segment].base_address;
        for (uint32 i = 0; i < numa_mem_affinity_count; i++) {
            uint64 base = numa_mem_affinities[i].base;
            uint64 length = numa_mem_affinities[i].length;
            if (ecam_base >= base && ecam_base < base + length) {
                *node_out = numa_mem_affinities[i].proximity_domain;
                return 1;
            }
        }
    }

    return 0;
}

const ACPIPCIEConfigSegment *acpi_pcie_ecam_segments(uint32 *count_out) {
    if (count_out) *count_out = pcie_segment_count;
    return pcie_segments;
}

int acpi_hpet_info(ACPIHPETInfo *out) {
    if (!out || !hpet_info.present) return 0;
    *out = hpet_info;
    return 1;
}

int acpi_pm_timer_info(ACPIPMTimerInfo *out) {
    if (!out || !pm_timer_info.present) return 0;
    *out = pm_timer_info;
    return 1;
}

int acpi_reset_reg_info(ACPIResetRegInfo *out) {
    if (!out || !reset_reg_info.present) return 0;
    *out = reset_reg_info;
    return 1;
}

const ACPIIOMMUUnit *acpi_iommu_units(uint32 *count_out) {
    if (count_out) *count_out = iommu_unit_count;
    return iommu_units;
}
