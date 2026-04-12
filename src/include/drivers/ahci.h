#ifndef DRIVERS_AHCI_H
#define DRIVERS_AHCI_H

#include <types.h>

// AHCI port device types
#define AHCI_DEV_NULL      0
#define AHCI_DEV_SATA      1
#define AHCI_DEV_SATAPI    2   // CD-ROM / DVD

// Device signatures
#define SATA_SIG_ATA       0x00000101
#define SATA_SIG_ATAPI     0xEB140101
#define SATA_SIG_SEMB      0xC33C0101
#define SATA_SIG_PM        0x96690101

// SATA status: DET (Device Detection)
#define HBA_PORT_DET_PRESENT  3
#define HBA_PORT_IPM_ACTIVE   1

#define AHCI_MAX_PORTS     32

typedef struct {
    uint8   type;           // AHCI_DEV_*
    uint8   port_num;       // HBA port index
    uint8   present;        // 1 = device detected
    uint8   reserved;
    char    model[41];      // model string from IDENTIFY
    uint64  sector_count;   // total sectors
    uint16  sector_size;    // bytes per sector (512 for SATA, 2048 for ATAPI)
} AHCIDevice;

void ahci_init(void);
uint32 ahci_device_count(void);
const AHCIDevice *ahci_get_device(uint32 idx);

// Read sectors from an AHCI device.
// For SATA: lba and count in 512-byte sectors.
// For SATAPI (CD-ROM): lba and count in 2048-byte sectors.
// Returns 0 on success, -1 on error.
int ahci_read_sectors(uint32 dev_idx, uint64 lba, uint32 count, void *buf);

#endif
