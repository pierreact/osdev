// SRAT + SLIT parsers and NUMA query accessors.
// Also hosts acpi_pci_to_node, which composes SRAT Type 5, AML _BBN/_PXM
// (via arch/aml.h), and MCFG ECAM fallback.

#include <types.h>
#include <arch/acpi.h>
#include <arch/aml.h>
#include "acpi_internal.h"

ACPINumaCpuAffinity numa_cpu_affinities[MAX_NUMA_CPU_AFFINITIES];
uint32 numa_cpu_affinity_count = 0;
ACPINumaMemAffinity numa_mem_affinities[MAX_NUMA_MEM_AFFINITIES];
uint32 numa_mem_affinity_count = 0;
ACPINumaPCIAffinity numa_pci_affinities[MAX_NUMA_PCI_AFFINITIES];
uint32 numa_pci_affinity_count = 0;
uint32 numa_nodes[MAX_NUMA_NODES];
uint32 numa_node_count = 0;
uint8  slit_distances[MAX_NUMA_NODES * MAX_NUMA_NODES];
uint8  slit_present = 0;

// Preserve original reset semantics: numa_pci_affinity_count is not
// cleared here (original reset_parsed_state behavior).
void acpi_numa_reset(void) {
    numa_cpu_affinity_count = 0;
    numa_mem_affinity_count = 0;
    numa_node_count = 0;
    slit_present = 0;
}

static int append_numa_node(uint32 node_id) {
    for (uint32 i = 0; i < numa_node_count; i++) {
        if (numa_nodes[i] == node_id) return 0;
    }
    if (numa_node_count >= MAX_NUMA_NODES) return 1;
    numa_nodes[numa_node_count++] = node_id;
    return 0;
}

void parse_srat(ACPISDTHeader *hdr) {
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

void parse_slit(ACPISDTHeader *hdr) {
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
