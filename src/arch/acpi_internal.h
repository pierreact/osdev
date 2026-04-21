#ifndef ARCH_ACPI_INTERNAL_H
#define ARCH_ACPI_INTERNAL_H

// Internal contract between acpi*.c translation units. Not installed
// under include/; consumers outside src/arch/ must use <arch/acpi.h>.

#include <types.h>
#include <arch/acpi.h>

// ============================================================================
// Shared packed structs used by multiple parsers
// ============================================================================

// ACPI Generic Address Structure (used by FADT fields and parse_hpet).
typedef struct __attribute__((packed)) {
    uint8  space_id;
    uint8  bit_width;
    uint8  bit_offset;
    uint8  access_size;
    uint64 address;
} ACPIGAS;

// Fixed ACPI Description Table layout. Needed by acpi.c (to reach DSDT
// for the AML bridge) and by parse_fadt in acpi_platform.c.
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

// ============================================================================
// Shared utilities (defined in acpi.c)
// ============================================================================
int   memcmp_bytes(const void *a, const void *b, uint32 len);
uint8 checksum_bytes(const void *ptr, uint32 len);
int   signatures_equal(const char *a, const char *b);
int   validate_sdt(ACPISDTHeader *h);

// ============================================================================
// RSDP state (defined in acpi.c, consumed by acpi_lsacpi.c)
// ============================================================================
extern RSDP   *saved_rsdp;
extern uint64  saved_rsdt_xsdt_addr;
extern uint8   saved_root_is_xsdt;

// ============================================================================
// MADT stats (defined in acpi_madt.c, printed by acpi_init)
// ============================================================================
extern uint32 madt_type0_seen;
extern uint32 madt_type0_usable;
extern uint32 madt_type9_seen;
extern uint32 madt_type9_usable;

// ============================================================================
// NUMA caches (defined in acpi_numa.c)
// ============================================================================
extern ACPINumaCpuAffinity numa_cpu_affinities[MAX_NUMA_CPU_AFFINITIES];
extern uint32 numa_cpu_affinity_count;
extern ACPINumaMemAffinity numa_mem_affinities[MAX_NUMA_MEM_AFFINITIES];
extern uint32 numa_mem_affinity_count;
extern ACPINumaPCIAffinity numa_pci_affinities[MAX_NUMA_PCI_AFFINITIES];
extern uint32 numa_pci_affinity_count;
extern uint32 numa_nodes[MAX_NUMA_NODES];
extern uint32 numa_node_count;
extern uint8  slit_distances[MAX_NUMA_NODES * MAX_NUMA_NODES];
extern uint8  slit_present;

// ============================================================================
// Platform caches (defined in acpi_platform.c)
// ============================================================================
extern ACPIPCIEConfigSegment pcie_segments[MAX_PCIE_SEGMENTS];
extern uint32 pcie_segment_count;
extern ACPIHPETInfo hpet_info;
extern ACPIPMTimerInfo pm_timer_info;
extern ACPIResetRegInfo reset_reg_info;
extern ACPIIOMMUUnit iommu_units[MAX_IOMMU_UNITS];
extern uint32 iommu_unit_count;

// ============================================================================
// Per-table parsers (registered in acpi.c's table_parsers[])
// ============================================================================
void parse_madt(ACPISDTHeader *hdr);
void parse_srat(ACPISDTHeader *hdr);
void parse_slit(ACPISDTHeader *hdr);
void parse_fadt(ACPISDTHeader *hdr);
void parse_hpet(ACPISDTHeader *hdr);
void parse_mcfg(ACPISDTHeader *hdr);
void parse_dmar(ACPISDTHeader *hdr);
void parse_ivrs(ACPISDTHeader *hdr);

// ============================================================================
// Per-module reset hooks. acpi.c's reset_parsed_state() calls these so each
// module zeros the state it owns before a fresh acpi_init.
// ============================================================================
void acpi_madt_reset(void);
void acpi_numa_reset(void);
void acpi_platform_reset(void);

#endif // ARCH_ACPI_INTERNAL_H
