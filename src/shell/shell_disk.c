// Disk/block-device shell commands: sys.disk.ls, sys.disk.info,
// sys.disk.read, sys.disk.write.

#include "shell_internal.h"
#include <drivers/ahci.h>

void cmd_lsblk(void) {
    sh_print("NAME    TYPE     SIZE       MODEL\n");
    int found = 0;

    // IDE devices
    uint32 sectors = sh_ide_sectors();
    if (sectors > 0) {
        uint32 size_mb = sectors / 2048;
        sh_print("hda     disk     ");
        sh_print_dec(size_mb);
        sh_print(" MB     ");
        sh_print(sh_ide_model());
        sh_putc('\n');
        found++;
    }

    // AHCI devices
    uint32 ahci_count = ahci_device_count();
    for (uint32 i = 0; i < ahci_count; i++) {
        const AHCIDevice *dev = ahci_get_device(i);
        if (!dev || !dev->present) continue;
        sh_print("sata");
        sh_print_dec(i);
        if (dev->type == AHCI_DEV_SATAPI)
            sh_print("   cdrom    ");
        else {
            sh_print("   disk     ");
        }
        if (dev->sector_count > 0) {
            uint64 size_mb = (dev->sector_count * dev->sector_size) / (1024 * 1024);
            sh_print_dec(size_mb);
            sh_print(" MB     ");
        } else {
            sh_print("-          ");
        }
        sh_print(dev->model[0] ? (char *)dev->model : "?");
        sh_putc('\n');
        found++;
    }

    if (found == 0)
        sh_print("No drives detected\n");
}

void cmd_diskinfo(void) {
    sh_print("Disk Information:\n");
    uint32 sectors = sh_ide_sectors();
    if (sectors == 0) {
        sh_print("No drive detected\n");
        return;
    }
    sh_print("Drive: Primary Master\n");
    sh_print("Model: ");
    sh_print(sh_ide_model());
    sh_putc('\n');
    sh_print("Sectors: ");
    sh_print_dec(sectors);
    sh_putc('\n');
    uint32 size_mb = sectors / 2048;
    sh_print("Size: ");
    sh_print_dec(size_mb);
    sh_print(" MB\n");
}

void cmd_diskread(void) {
    char *args = cmd_buffer + 13;
    while (*args == ' ') args++;
    if (*args == '\0') {
        sh_print("Usage: sys.disk.read <lba> [count]\n");
        return;
    }
    uint32 lba = parse_number(args);
    while (*args && *args != ' ') args++;
    while (*args == ' ') args++;
    uint32 count = (*args) ? parse_number(args) : 1;
    if (count > 8) count = 8;

    uint8 *buffer = (uint8*)sh_malloc(count * 512);
    if (!buffer) {
        sh_print("Failed to allocate buffer\n");
        return;
    }
    sh_print("Reading ");
    sh_print_hex(count, " sector(s) from LBA ");
    sh_print_hex(lba, "\n");

    if (sh_ide_read(lba, count, buffer) != 0) {
        sh_print("Read failed\n");
        sh_free(buffer);
        return;
    }
    hex_dump(buffer, count * 512);
    sh_free(buffer);
}

void cmd_diskwrite(void) {
    char *args = cmd_buffer + 14;
    while (*args == ' ') args++;
    if (*args == '\0') {
        sh_print("Usage: sys.disk.write <lba> <data>\n");
        return;
    }
    uint32 lba = parse_number(args);
    while (*args && *args != ' ') args++;
    while (*args == ' ') args++;
    if (*args == '\0') {
        sh_print("No data specified\n");
        return;
    }

    uint8 *buffer = (uint8*)sh_malloc(512);
    if (!buffer) {
        sh_print("Failed to allocate buffer\n");
        return;
    }
    for (int i = 0; i < 512; i++) buffer[i] = 0;
    int len = 0;
    while (args[len] && len < 512) {
        buffer[len] = args[len];
        len++;
    }
    sh_print("Writing to LBA ");
    sh_print_hex(lba, "\n");
    if (sh_ide_write(lba, buffer) != 0)
        sh_print("Write failed\n");
    else
        sh_print("Write complete\n");
    sh_free(buffer);
}
