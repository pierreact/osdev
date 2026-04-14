#ifndef IDE_H
#define IDE_H
#include <types.h>

void ide_init();
int ide_read_sector(uint32 lba, uint8 *buffer);
int ide_write_sector(uint32 lba, uint8 *buffer);
int ide_read_sectors(uint32 lba, uint8 count, uint8 *buffer);
int ide_write_sectors(uint32 lba, uint8 count, uint8 *buffer);
int ide_read_sectors_drive(uint8 drive, uint32 lba, uint8 count, uint8 *buffer);
uint32 ide_get_sector_count();
char* ide_get_model();

#endif
