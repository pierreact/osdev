#include <drivers/ide.h>
#include <drivers/monitor.h>
#include <arch/ports.h>

// Primary IDE controller ports
#define IDE_DATA        0x1F0
#define IDE_ERROR       0x1F1
#define IDE_FEATURES    0x1F1
#define IDE_SECTOR_CNT  0x1F2
#define IDE_LBA_LOW     0x1F3
#define IDE_LBA_MID     0x1F4
#define IDE_LBA_HIGH    0x1F5
#define IDE_DRIVE_HEAD  0x1F6
#define IDE_STATUS      0x1F7
#define IDE_COMMAND     0x1F7
#define IDE_CONTROL     0x3F6
#define IDE_ALT_STATUS  0x3F6

// Status register bits
#define IDE_SR_BSY      0x80
#define IDE_SR_DRDY     0x40
#define IDE_SR_DF       0x20
#define IDE_SR_DSC      0x10
#define IDE_SR_DRQ      0x08
#define IDE_SR_CORR     0x04
#define IDE_SR_IDX      0x02
#define IDE_SR_ERR      0x01

// Commands
#define IDE_CMD_READ_PIO        0x20
#define IDE_CMD_WRITE_PIO       0x30
#define IDE_CMD_IDENTIFY        0xEC

// Drive info
static uint32 drive_sectors = 0;
static char drive_model[41];

static void ide_wait_bsy() {
    while(inb(IDE_STATUS) & IDE_SR_BSY);
}

static void ide_wait_drq() {
    while(!(inb(IDE_STATUS) & IDE_SR_DRQ));
}

static int ide_wait_ready() {
    uint8 status;
    int timeout = 100000;
    
    while(timeout--) {
        status = inb(IDE_STATUS);
        if(!(status & IDE_SR_BSY) && (status & IDE_SR_DRDY)) {
            return 0;
        }
    }
    return -1;
}

void ide_init() {
    kprint("Initializing IDE controller...\n");

    // Check if IDE controller exists (Q35/AHCI returns 0xFF)
    uint8 probe = inb(IDE_STATUS);
    if(probe == 0xFF) {
        kprint("IDE: No controller present\n");
        return;
    }

    outb(IDE_CONTROL, 0x02);

    ide_wait_bsy();

    outb(IDE_DRIVE_HEAD, 0xA0);
    outb(IDE_SECTOR_CNT, 0);
    outb(IDE_LBA_LOW, 0);
    outb(IDE_LBA_MID, 0);
    outb(IDE_LBA_HIGH, 0);
    outb(IDE_COMMAND, IDE_CMD_IDENTIFY);

    uint8 status = inb(IDE_STATUS);
    if(status == 0) {
        kprint("IDE: No drive detected\n");
        return;
    }
    
    ide_wait_bsy();
    
    status = inb(IDE_STATUS);
    if(status & IDE_SR_ERR) {
        kprint("IDE: Error during identify\n");
        return;
    }
    
    ide_wait_drq();
    
    uint16 identify_data[256];
    for(int i = 0; i < 256; i++) {
        identify_data[i] = inw(IDE_DATA);
    }
    
    drive_sectors = *((uint32*)&identify_data[60]);
    
    for(int i = 0; i < 20; i++) {
        drive_model[i*2] = (identify_data[27+i] >> 8) & 0xFF;
        drive_model[i*2+1] = identify_data[27+i] & 0xFF;
    }
    drive_model[40] = '\0';
    
    kprint("IDE: Drive detected\n");
    kprint("Model: ");
    kprint(drive_model);
    putc('\n');
    kprint_dec(drive_sectors);
    kprint(" sectors\n");
}

static int ide_read_sector_drive(uint8 drive, uint32 lba, uint8 *buffer) {
    if(ide_wait_ready() != 0) {
        return -1;
    }

    outb(IDE_DRIVE_HEAD, 0xE0 | ((drive & 1) << 4) | ((lba >> 24) & 0x0F));
    outb(IDE_SECTOR_CNT, 1);
    outb(IDE_LBA_LOW, lba & 0xFF);
    outb(IDE_LBA_MID, (lba >> 8) & 0xFF);
    outb(IDE_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(IDE_COMMAND, IDE_CMD_READ_PIO);

    ide_wait_drq();

    uint16 *buf16 = (uint16*)buffer;
    for(int i = 0; i < 256; i++) {
        buf16[i] = inw(IDE_DATA);
    }

    return 0;
}

int ide_read_sector(uint32 lba, uint8 *buffer) {
    return ide_read_sector_drive(0, lba, buffer);
}

int ide_write_sector(uint32 lba, uint8 *buffer) {
    if(ide_wait_ready() != 0) {
        return -1;
    }
    
    outb(IDE_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    outb(IDE_SECTOR_CNT, 1);
    outb(IDE_LBA_LOW, lba & 0xFF);
    outb(IDE_LBA_MID, (lba >> 8) & 0xFF);
    outb(IDE_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(IDE_COMMAND, IDE_CMD_WRITE_PIO);
    
    ide_wait_drq();
    
    uint16 *buf16 = (uint16*)buffer;
    for(int i = 0; i < 256; i++) {
        outw(IDE_DATA, buf16[i]);
    }
    
    ide_wait_bsy();
    
    return 0;
}

int ide_read_sectors(uint32 lba, uint8 count, uint8 *buffer) {
    return ide_read_sectors_drive(0, lba, count, buffer);
}

int ide_read_sectors_drive(uint8 drive, uint32 lba, uint8 count, uint8 *buffer) {
    for(uint8 i = 0; i < count; i++) {
        if(ide_read_sector_drive(drive, lba + i, buffer + (i * 512)) != 0) {
            return -1;
        }
    }
    return 0;
}

int ide_write_sectors(uint32 lba, uint8 count, uint8 *buffer) {
    for(uint8 i = 0; i < count; i++) {
        if(ide_write_sector(lba + i, buffer + (i * 512)) != 0) {
            return -1;
        }
    }
    return 0;
}

uint32 ide_get_sector_count() {
    return drive_sectors;
}

char* ide_get_model() {
    return drive_model;
}
