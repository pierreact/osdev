#include <kernel/bootmedia.h>
#include <drivers/ahci.h>
#include <drivers/ide.h>
#include <fs/iso9660.h>
#include <fs/fat32.h>
#include <fs/vfs.h>
#include <drivers/monitor.h>

// Scan for boot media and mount filesystems:
// - ISO 9660 (from AHCI SATAPI) at /
// - FAT32 (from IDE or AHCI SATA) at /mnt/<volume_name>
void bootmedia_init(void) {
    vfs_init();

    // Mount ISO from AHCI SATAPI (CD-ROM)
    uint32 ahci_count = ahci_device_count();
    for (uint32 i = 0; i < ahci_count; i++) {
        const AHCIDevice *dev = ahci_get_device(i);
        if (!dev || !dev->present) continue;
        if (dev->type != AHCI_DEV_SATAPI) continue;

        if (iso9660_mount(i) == 0) {
            vfs_mount("/", FS_TYPE_ISO9660, i);
            break;
        }
    }

    // Mount FAT32 from IDE (if available)
    uint32 ide_sectors = ide_get_sector_count();
    if (ide_sectors > 0) {
        vfs_mount("/mnt/IDE", FS_TYPE_FAT32, 0);
    }

    if (!iso9660_mounted())
        kprint("BOOT: no ISO found on AHCI devices\n");
}
