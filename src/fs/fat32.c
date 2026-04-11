#include <fs/fat32.h>
#include <drivers/ide.h>
#include <drivers/monitor.h>
#include <kernel/mem.h>

#define FAT32_DRIVE 1

// BPB (BIOS Parameter Block) - first 90 bytes of sector 0
typedef struct __attribute__((packed)) {
    uint8  jmp[3];
    uint8  oem_name[8];
    uint16 bytes_per_sector;
    uint8  sectors_per_cluster;
    uint16 reserved_sectors;
    uint8  num_fats;
    uint16 root_entry_count;    // 0 for FAT32
    uint16 total_sectors_16;    // 0 for FAT32
    uint8  media_type;
    uint16 fat_size_16;         // 0 for FAT32
    uint16 sectors_per_track;
    uint16 num_heads;
    uint32 hidden_sectors;
    uint32 total_sectors_32;
    uint32 fat_size_32;
    uint16 ext_flags;
    uint16 fs_version;
    uint32 root_cluster;
    uint16 fs_info;
    uint16 backup_boot_sector;
    uint8  reserved[12];
    uint8  drive_number;
    uint8  reserved1;
    uint8  boot_sig;
    uint32 volume_id;
    uint8  volume_label[11];
    uint8  fs_type[8];
} FAT32_BPB;

// Directory entry - 32 bytes
typedef struct __attribute__((packed)) {
    uint8  name[11];
    uint8  attr;
    uint8  nt_reserved;
    uint8  create_time_tenths;
    uint16 create_time;
    uint16 create_date;
    uint16 access_date;
    uint16 cluster_hi;
    uint16 modify_time;
    uint16 modify_date;
    uint16 cluster_lo;
    uint32 file_size;
} FAT32_DirEntry;

// Directory entry attributes
#define ATTR_READ_ONLY  0x01
#define ATTR_HIDDEN     0x02
#define ATTR_SYSTEM     0x04
#define ATTR_VOLUME_ID  0x08
#define ATTR_DIRECTORY  0x10
#define ATTR_ARCHIVE    0x20
#define ATTR_LFN        0x0F

// Cached filesystem state
static struct {
    uint32 fat_start_lba;
    uint32 data_start_lba;
    uint32 root_cluster;
    uint8  sectors_per_cluster;
    int    valid;
} fat32_state;

static uint32 fat32_cluster_to_lba(uint32 cluster) {
    return fat32_state.data_start_lba +
           (cluster - 2) * fat32_state.sectors_per_cluster;
}

static uint32 fat32_read_fat_entry(uint32 cluster) {
    // Each FAT entry is 4 bytes. 128 entries per 512-byte sector.
    uint32 fat_sector = fat32_state.fat_start_lba + (cluster / 128);
    uint32 fat_offset = cluster % 128;

    uint8 *buf = (uint8 *)kmalloc(512);
    if (!buf) return 0x0FFFFFFF;

    if (ide_read_sectors_drive(FAT32_DRIVE, fat_sector, 1, buf) != 0) {
        kfree(buf);
        return 0x0FFFFFFF;
    }

    uint32 entry = ((uint32 *)buf)[fat_offset] & 0x0FFFFFFF;
    kfree(buf);
    return entry;
}

static int fat32_read_cluster(uint32 cluster, uint8 *buffer) {
    uint32 lba = fat32_cluster_to_lba(cluster);
    return ide_read_sectors_drive(FAT32_DRIVE, lba,
                                  fat32_state.sectors_per_cluster, buffer);
}

// Convert user filename to 8.3 format (11 bytes, space-padded, uppercase)
static void fat32_format_83name(const char *name, uint8 *out) {
    int i;
    for (i = 0; i < 11; i++) out[i] = ' ';

    // Find dot position
    int dot = -1;
    for (i = 0; name[i]; i++) {
        if (name[i] == '.') { dot = i; break; }
    }

    // Copy base name (up to 8 chars)
    int limit = dot >= 0 ? dot : i;
    if (limit > 8) limit = 8;
    for (int j = 0; j < limit; j++) {
        uint8 c = name[j];
        if (c >= 'a' && c <= 'z') c -= 32;
        out[j] = c;
    }

    // Copy extension (up to 3 chars)
    if (dot >= 0) {
        int ext_start = dot + 1;
        for (int j = 0; j < 3 && name[ext_start + j]; j++) {
            uint8 c = name[ext_start + j];
            if (c >= 'a' && c <= 'z') c -= 32;
            out[8 + j] = c;
        }
    }
}

static int fat32_names_equal(const uint8 *a, const uint8 *b) {
    for (int i = 0; i < 11; i++) {
        if (a[i] != b[i]) return 0;
    }
    return 1;
}

void fat32_init() {
    kprint("Initializing FAT32 driver...\n");

    fat32_state.valid = 0;

    uint8 *sector = (uint8 *)kmalloc(512);
    if (!sector) {
        kprint("FAT32: Failed to allocate buffer\n");
        return;
    }

    if (ide_read_sectors_drive(FAT32_DRIVE, 0, 1, sector) != 0) {
        kprint("FAT32: Failed to read boot sector\n");
        kfree(sector);
        return;
    }

    // Validate boot signature
    if (sector[510] != 0x55 || sector[511] != 0xAA) {
        kprint("FAT32: Invalid boot signature\n");
        kfree(sector);
        return;
    }

    FAT32_BPB *bpb = (FAT32_BPB *)sector;

    if (bpb->bytes_per_sector != 512) {
        kprint("FAT32: Unsupported sector size\n");
        kfree(sector);
        return;
    }

    fat32_state.sectors_per_cluster = bpb->sectors_per_cluster;
    fat32_state.fat_start_lba = bpb->reserved_sectors;
    fat32_state.data_start_lba = bpb->reserved_sectors +
                                  (bpb->num_fats * bpb->fat_size_32);
    fat32_state.root_cluster = bpb->root_cluster;
    fat32_state.valid = 1;

    kfree(sector);

    kprint("FAT32: Filesystem mounted (cluster size ");
    kprint_dec(fat32_state.sectors_per_cluster * 512);
    kprint(" bytes)\n");
}

void fat32_list_root() {
    if (!fat32_state.valid) {
        kprint("FAT32: No filesystem mounted\n");
        return;
    }

    uint32 cluster_size = fat32_state.sectors_per_cluster * 512;
    uint8 *buf = (uint8 *)kmalloc(cluster_size);
    if (!buf) {
        kprint("FAT32: Out of memory\n");
        return;
    }

    uint32 cluster = fat32_state.root_cluster;

    while (cluster < 0x0FFFFFF8) {
        if (fat32_read_cluster(cluster, buf) != 0) {
            kprint("FAT32: Read error\n");
            break;
        }

        uint32 entries = cluster_size / 32;
        FAT32_DirEntry *dir = (FAT32_DirEntry *)buf;

        for (uint32 i = 0; i < entries; i++) {
            if (dir[i].name[0] == 0x00) goto done;      // No more entries
            if (dir[i].name[0] == 0xE5) continue;        // Deleted
            if (dir[i].attr == ATTR_LFN) continue;       // Long filename entry
            if (dir[i].attr & ATTR_VOLUME_ID) continue;  // Volume label

            // Print filename (8 chars) and extension (3 chars)
            for (int j = 0; j < 8; j++) {
                if (dir[i].name[j] != ' ')
                    putc(dir[i].name[j]);
            }
            if (dir[i].name[8] != ' ') {
                putc('.');
                for (int j = 8; j < 11; j++) {
                    if (dir[i].name[j] != ' ')
                        putc(dir[i].name[j]);
                }
            }

            // Pad to column and print size or <DIR>
            kprint("  ");
            if (dir[i].attr & ATTR_DIRECTORY) {
                kprint("<DIR>");
            } else {
                kprint_dec(dir[i].file_size);
                kprint(" bytes");
            }
            putc('\n');
        }

        cluster = fat32_read_fat_entry(cluster);
    }

done:
    kfree(buf);
}

int fat32_cat_file(const char *name) {
    if (!fat32_state.valid) {
        kprint("FAT32: No filesystem mounted\n");
        return -1;
    }

    uint8 search_name[11];
    fat32_format_83name(name, search_name);

    uint32 cluster_size = fat32_state.sectors_per_cluster * 512;
    uint8 *buf = (uint8 *)kmalloc(cluster_size);
    if (!buf) {
        kprint("FAT32: Out of memory\n");
        return -1;
    }

    // Search root directory for the file
    uint32 file_cluster = 0;
    uint32 file_size = 0;
    uint32 cluster = fat32_state.root_cluster;

    while (cluster < 0x0FFFFFF8) {
        if (fat32_read_cluster(cluster, buf) != 0) {
            kfree(buf);
            return -1;
        }

        uint32 entries = cluster_size / 32;
        FAT32_DirEntry *dir = (FAT32_DirEntry *)buf;

        for (uint32 i = 0; i < entries; i++) {
            if (dir[i].name[0] == 0x00) break;
            if (dir[i].name[0] == 0xE5) continue;
            if (dir[i].attr == ATTR_LFN) continue;
            if (dir[i].attr & ATTR_VOLUME_ID) continue;
            if (dir[i].attr & ATTR_DIRECTORY) continue;

            if (fat32_names_equal(dir[i].name, search_name)) {
                file_cluster = ((uint32)dir[i].cluster_hi << 16) |
                               dir[i].cluster_lo;
                file_size = dir[i].file_size;
                goto found;
            }
        }

        cluster = fat32_read_fat_entry(cluster);
    }

    kprint("File not found: ");
    kprint(name);
    putc('\n');
    kfree(buf);
    return -1;

found:
    // Read and print file contents cluster by cluster
    cluster = file_cluster;
    uint32 remaining = file_size;

    while (remaining > 0 && cluster < 0x0FFFFFF8) {
        if (fat32_read_cluster(cluster, buf) != 0) {
            kprint("\nFAT32: Read error\n");
            break;
        }

        uint32 to_print = remaining < cluster_size ? remaining : cluster_size;
        for (uint32 i = 0; i < to_print; i++) {
            putc(buf[i]);
        }
        remaining -= to_print;

        cluster = fat32_read_fat_entry(cluster);
    }

    kfree(buf);
    return 0;
}
