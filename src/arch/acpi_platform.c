// Platform descriptor parsers: FADT (PM timer, reset register),
// HPET, MCFG (PCIe ECAM), DMAR (Intel IOMMU), IVRS (AMD IOMMU).

#include <types.h>
#include <arch/acpi.h>
#include "acpi_internal.h"

ACPIPCIEConfigSegment pcie_segments[MAX_PCIE_SEGMENTS];
uint32 pcie_segment_count = 0;

ACPIHPETInfo hpet_info = {0};
ACPIPMTimerInfo pm_timer_info = {0};
ACPIResetRegInfo reset_reg_info = {0};

ACPIIOMMUUnit iommu_units[MAX_IOMMU_UNITS];
uint32 iommu_unit_count = 0;

void acpi_platform_reset(void) {
    pcie_segment_count = 0;
    hpet_info.present = 0;
    pm_timer_info.present = 0;
    reset_reg_info.present = 0;
    iommu_unit_count = 0;
}

void parse_fadt(ACPISDTHeader *hdr) {
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

void parse_hpet(ACPISDTHeader *hdr) {
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

void parse_mcfg(ACPISDTHeader *hdr) {
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

void parse_dmar(ACPISDTHeader *hdr) {
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

void parse_ivrs(ACPISDTHeader *hdr) {
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
