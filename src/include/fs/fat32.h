#ifndef FAT32_H
#define FAT32_H
#include <types.h>

void fat32_init();
void fat32_list_root();
int fat32_cat_file(const char *name);

#endif
