#ifndef FS_ISO9660_H
#define FS_ISO9660_H

#include <types.h>

#define ISO_SECTOR_SIZE 2048

// Mount the ISO 9660 filesystem from an AHCI device index.
// Returns 0 on success, -1 on error.
int iso9660_mount(uint32 ahci_dev_idx);

// Check if an ISO is currently mounted.
int iso9660_mounted(void);

// List directory contents. Prints to console.
// path = "/" for root, "/bin", etc.
void iso9660_ls(const char *path);

// Read a file into buf. Returns bytes read, or -1 on error.
// buf must be large enough. max_size limits the read.
int iso9660_read_file(const char *path, void *buf, uint32 max_size);

// Get file size. Returns size in bytes, or 0 if not found.
uint32 iso9660_file_size(const char *path);

#endif
