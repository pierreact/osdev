#ifndef ARCH_AML_H
#define ARCH_AML_H

#include <types.h>

// Subset AML walker for extracting NUMA proximity (_PXM) and bus base
// number (_BBN) from DSDT/SSDT tables.
//
// This is NOT a full AML interpreter. It walks the AML grammar enough
// to identify Device scopes and Name(_BBN, ...) / Name(_PXM, ...) /
// Method(_PXM, 0) {Return(constant)} declarations. Anything more complex
// (computed methods, conditional logic) is silently ignored.
//
// Used to discover PCI host bridge proximity domains, since QEMU's
// pxb-pcie does not emit SRAT Generic Initiator entries.

#define MAX_AML_HOST_BRIDGES 16

typedef struct {
    uint8  bus_base;        // _BBN value
    uint8  has_bbn;
    uint32 proximity;       // _PXM value
    uint8  has_pxm;
} AMLHostBridge;

// Walk one AML byte stream (DSDT or SSDT body, after the SDT header).
// Calls discover_host_bridges() to populate the static table.
void aml_parse(const uint8 *body, uint32 length);

// Look up the proximity domain for a PCI bus number.
// Returns 1 on success, 0 if not found.
int aml_bus_to_node(uint8 bus, uint32 *node_out);

// Number of host bridges discovered.
uint32 aml_host_bridge_count(void);
const AMLHostBridge *aml_host_bridge(uint32 idx);

#endif
