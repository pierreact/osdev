#ifndef FS_VFS_H
#define FS_VFS_H

#include <types.h>

#define VFS_MAX_MOUNTS 8

#define FS_TYPE_ISO9660  1
#define FS_TYPE_FAT32    2

typedef struct {
    char    mountpoint[64];   // e.g., "/", "/mnt/MYOS"
    uint8   fs_type;          // FS_TYPE_*
    uint32  device;           // AHCI device idx or IDE drive
    uint8   active;
} VFSMount;

// Initialize VFS (clear mount table)
void vfs_init(void);

// Mount a filesystem at a path
int vfs_mount(const char *mountpoint, uint8 fs_type, uint32 device);

// List directory contents (prints to console)
void vfs_ls(const char *path);

// Read a file into buf. Returns bytes read, or -1 on error.
int vfs_read_file(const char *path, void *buf, uint32 max_size);

// Get file size. Returns 0 if not found.
uint32 vfs_file_size(const char *path);

// Number of active mounts
uint32 vfs_mount_count(void);
const VFSMount *vfs_get_mount(uint32 idx);

#endif
