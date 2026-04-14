#ifndef KERNEL_BOOTMEDIA_H
#define KERNEL_BOOTMEDIA_H

#include <types.h>

// Scan AHCI devices for the boot ISO (volume label "ISURUS_OS")
// and mount it via iso9660_mount().
void bootmedia_init(void);

#endif
