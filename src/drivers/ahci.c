#include <drivers/ahci.h>
#include <drivers/pci.h>
#include <kernel/mem.h>
#include <drivers/monitor.h>

// HBA memory registers (at ABAR)
#define HBA_CAP     0x00
#define HBA_GHC     0x04
#define HBA_IS      0x08
#define HBA_PI      0x0C
#define HBA_VS      0x10
#define HBA_CAP2    0x24
#define HBA_BOHC    0x28

// GHC bits
#define GHC_HR      (1u << 0)   // HBA reset
#define GHC_IE      (1u << 1)   // Interrupt enable
#define GHC_AE      (1u << 31)  // AHCI enable

// Per-port register offsets (base = ABAR + 0x100 + port*0x80)
#define PORT_CLB    0x00   // Command List Base (low)
#define PORT_CLBU   0x04   // Command List Base (high)
#define PORT_FB     0x08   // FIS Base (low)
#define PORT_FBU    0x0C   // FIS Base (high)
#define PORT_IS     0x10   // Interrupt Status
#define PORT_IE     0x14   // Interrupt Enable
#define PORT_CMD    0x18   // Command and Status
#define PORT_TFD    0x20   // Task File Data
#define PORT_SIG    0x24   // Signature
#define PORT_SSTS   0x28   // SATA Status
#define PORT_SCTL   0x2C   // SATA Control
#define PORT_SERR   0x30   // SATA Error
#define PORT_SACT   0x34   // SATA Active
#define PORT_CI     0x38   // Command Issue

// PORT_CMD bits
#define PORT_CMD_ST   (1u << 0)    // Start
#define PORT_CMD_FRE  (1u << 4)    // FIS Receive Enable
#define PORT_CMD_FR   (1u << 14)   // FIS Receive Running
#define PORT_CMD_CR   (1u << 15)   // Command List Running

// ATA commands
#define ATA_CMD_IDENTIFY         0xEC
#define ATA_CMD_IDENTIFY_PACKET  0xA1
#define ATA_CMD_READ_DMA_EXT     0x25
#define ATA_CMD_PACKET           0xA0

// FIS types
#define FIS_TYPE_REG_H2D  0x27  // Register FIS - Host to Device

// Command header flags
#define CMD_HDR_CFL_5     5     // Command FIS Length = 5 DWORDs
#define CMD_HDR_WRITE     (1 << 6)
#define CMD_HDR_ATAPI     (1 << 5)
#define CMD_HDR_PREFETCH  (1 << 1)

// Command header (32 bytes each, 32 per port)
typedef struct __attribute__((packed)) {
    uint16 flags;           // CFL:5, A, W, P, R, B, C, PMP:4
    uint16 prdtl;           // Physical Region Descriptor Table Length
    uint32 prdbc;           // PRD Byte Count transferred
    uint64 ctba;            // Command Table Base Address (128-aligned)
    uint32 reserved[4];
} CmdHeader;

// Physical Region Descriptor Table entry
typedef struct __attribute__((packed)) {
    uint64 dba;             // Data Base Address (2-byte aligned)
    uint32 reserved;
    uint32 dbc;             // Byte Count (bit 31 = interrupt on completion)
} PRDTEntry;

// Command Table (at ctba)
typedef struct __attribute__((packed)) {
    uint8     cfis[64];     // Command FIS
    uint8     acmd[16];     // ATAPI command
    uint8     reserved[48];
    PRDTEntry prdt[8];      // PRD table (up to 8 entries for our use)
} CmdTable;

// Register FIS - Host to Device
typedef struct __attribute__((packed)) {
    uint8  fis_type;        // FIS_TYPE_REG_H2D
    uint8  pmport_c;        // PM Port + C bit (bit 7 = 1 for command)
    uint8  command;
    uint8  featurel;
    uint8  lba0, lba1, lba2;
    uint8  device;
    uint8  lba3, lba4, lba5;
    uint8  featureh;
    uint16 count;
    uint8  icc;
    uint8  control;
    uint32 reserved;
} FIS_REG_H2D;

// Received FIS buffer (256 bytes per port)
typedef struct __attribute__((packed)) {
    uint8 dsfis[28];
    uint8 reserved0[4];
    uint8 psfis[20];
    uint8 reserved1[12];
    uint8 rfis[20];
    uint8 reserved2[4];
    uint8 sdbfis[8];
    uint8 ufis[64];
    uint8 reserved3[96];
} ReceivedFIS;

static volatile uint8 *abar = NULL;
static AHCIDevice devices[AHCI_MAX_PORTS];
static uint32 device_count = 0;

// Per-port DMA structures (allocated from high memory)
static CmdHeader *cmd_lists[AHCI_MAX_PORTS];
static ReceivedFIS *fis_bufs[AHCI_MAX_PORTS];
static CmdTable *cmd_tables[AHCI_MAX_PORTS];

static uint32 hba_read(uint32 reg) {
    return *(volatile uint32 *)(abar + reg);
}

static void hba_write(uint32 reg, uint32 val) {
    *(volatile uint32 *)(abar + reg) = val;
}

static volatile uint8 *port_base(uint8 port) {
    return abar + 0x100 + (uint32)port * 0x80;
}

static uint32 port_read(uint8 port, uint32 reg) {
    return *(volatile uint32 *)(port_base(port) + reg);
}

static void port_write(uint8 port, uint32 reg, uint32 val) {
    *(volatile uint32 *)(port_base(port) + reg) = val;
}

// Stop command engine on a port
static void port_stop(uint8 port) {
    uint32 cmd = port_read(port, PORT_CMD);
    cmd &= ~PORT_CMD_ST;
    cmd &= ~PORT_CMD_FRE;
    port_write(port, PORT_CMD, cmd);

    // Wait until CR and FR are cleared
    int timeout = 500000;
    while (timeout-- > 0) {
        cmd = port_read(port, PORT_CMD);
        if (!(cmd & PORT_CMD_CR) && !(cmd & PORT_CMD_FR))
            return;
    }
}

// Start command engine on a port
static void port_start(uint8 port) {
    // Wait until CR is cleared
    int timeout = 500000;
    while (timeout-- > 0) {
        if (!(port_read(port, PORT_CMD) & PORT_CMD_CR))
            break;
    }

    uint32 cmd = port_read(port, PORT_CMD);
    cmd |= PORT_CMD_FRE;
    cmd |= PORT_CMD_ST;
    port_write(port, PORT_CMD, cmd);
}

// Initialize a port's DMA structures
static void port_init_dma(uint8 port) {
    port_stop(port);

    // Allocate command list (1KB, 1KB-aligned)
    uint64 cl_phys = alloc_pages(1);
    memset((void *)cl_phys, 0, 4096);
    cmd_lists[port] = (CmdHeader *)cl_phys;

    // Allocate received FIS buffer (256 bytes, 256-aligned)
    uint64 fb_phys = alloc_pages(1);
    memset((void *)fb_phys, 0, 4096);
    fis_bufs[port] = (ReceivedFIS *)fb_phys;

    // Allocate one command table (128-aligned)
    uint64 ct_phys = alloc_pages(1);
    memset((void *)ct_phys, 0, 4096);
    cmd_tables[port] = (CmdTable *)ct_phys;

    // Set command list base
    port_write(port, PORT_CLB, (uint32)(cl_phys & 0xFFFFFFFF));
    port_write(port, PORT_CLBU, (uint32)(cl_phys >> 32));

    // Set FIS base
    port_write(port, PORT_FB, (uint32)(fb_phys & 0xFFFFFFFF));
    port_write(port, PORT_FBU, (uint32)(fb_phys >> 32));

    // Point command header 0 at our command table
    cmd_lists[port][0].ctba = ct_phys;

    // Clear pending interrupts and errors
    port_write(port, PORT_SERR, port_read(port, PORT_SERR));
    port_write(port, PORT_IS, port_read(port, PORT_IS));

    port_start(port);
}

// Issue a command on slot 0 and wait for completion
static int port_issue_cmd(uint8 port) {
    // Clear interrupt status
    port_write(port, PORT_IS, (uint32)-1);

    // Issue command on slot 0
    port_write(port, PORT_CI, 1);

    // Poll for completion
    int timeout = 1000000;
    while (timeout-- > 0) {
        uint32 ci = port_read(port, PORT_CI);
        if (!(ci & 1)) return 0;  // slot 0 completed

        uint32 is = port_read(port, PORT_IS);
        if (is & (1u << 30)) return -1;  // task file error
    }
    return -1;  // timeout
}

// Build and issue an ATA command (Register FIS H2D)
static int ata_command(uint8 port, uint8 cmd, uint64 lba, uint16 count,
                       uint64 buf_phys, uint32 buf_size, uint8 is_atapi) {
    CmdHeader *hdr = &cmd_lists[port][0];
    CmdTable *tbl = cmd_tables[port];

    memset(tbl, 0, sizeof(CmdTable));

    // Build command FIS
    FIS_REG_H2D *fis = (FIS_REG_H2D *)tbl->cfis;
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->pmport_c = 0x80;  // C bit = 1 (command, not control)
    fis->command = cmd;
    fis->device = (1 << 6);  // LBA mode
    fis->lba0 = (uint8)(lba & 0xFF);
    fis->lba1 = (uint8)((lba >> 8) & 0xFF);
    fis->lba2 = (uint8)((lba >> 16) & 0xFF);
    fis->lba3 = (uint8)((lba >> 24) & 0xFF);
    fis->lba4 = (uint8)((lba >> 32) & 0xFF);
    fis->lba5 = (uint8)((lba >> 40) & 0xFF);
    fis->count = count;

    // PRDT entry 0
    tbl->prdt[0].dba = buf_phys;
    tbl->prdt[0].dbc = (buf_size - 1);  // byte count minus 1

    // Command header
    hdr->flags = CMD_HDR_CFL_5;
    if (is_atapi) hdr->flags |= CMD_HDR_ATAPI;
    hdr->prdtl = 1;
    hdr->prdbc = 0;

    return port_issue_cmd(port);
}

// Issue ATAPI PACKET command (e.g., READ(12) for CD-ROM)
static int atapi_read(uint8 port, uint64 lba, uint32 count,
                      uint64 buf_phys, uint32 buf_size) {
    CmdHeader *hdr = &cmd_lists[port][0];
    CmdTable *tbl = cmd_tables[port];

    memset(tbl, 0, sizeof(CmdTable));

    // Build command FIS for PACKET command
    FIS_REG_H2D *fis = (FIS_REG_H2D *)tbl->cfis;
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->pmport_c = 0x80;
    fis->command = ATA_CMD_PACKET;
    fis->featurel = 1;  // DMA
    fis->lba1 = (uint8)(buf_size & 0xFF);         // byte count low
    fis->lba2 = (uint8)((buf_size >> 8) & 0xFF);  // byte count high

    // Build ATAPI command (SCSI READ(12))
    tbl->acmd[0] = 0xA8;  // READ(12)
    tbl->acmd[2] = (uint8)((lba >> 24) & 0xFF);
    tbl->acmd[3] = (uint8)((lba >> 16) & 0xFF);
    tbl->acmd[4] = (uint8)((lba >> 8) & 0xFF);
    tbl->acmd[5] = (uint8)(lba & 0xFF);
    tbl->acmd[6] = (uint8)((count >> 24) & 0xFF);
    tbl->acmd[7] = (uint8)((count >> 16) & 0xFF);
    tbl->acmd[8] = (uint8)((count >> 8) & 0xFF);
    tbl->acmd[9] = (uint8)(count & 0xFF);

    // PRDT
    tbl->prdt[0].dba = buf_phys;
    tbl->prdt[0].dbc = (buf_size - 1);

    // Command header
    hdr->flags = CMD_HDR_CFL_5 | CMD_HDR_ATAPI;
    hdr->prdtl = 1;
    hdr->prdbc = 0;

    return port_issue_cmd(port);
}

// Detect device type on a port
static uint8 detect_port_type(uint8 port) {
    uint32 ssts = port_read(port, PORT_SSTS);
    uint8 det = ssts & 0x0F;
    uint8 ipm = (ssts >> 8) & 0x0F;

    if (det != HBA_PORT_DET_PRESENT || ipm != HBA_PORT_IPM_ACTIVE)
        return AHCI_DEV_NULL;

    uint32 sig = port_read(port, PORT_SIG);
    switch (sig) {
    case SATA_SIG_ATAPI: return AHCI_DEV_SATAPI;
    case SATA_SIG_ATA:   return AHCI_DEV_SATA;
    default:             return AHCI_DEV_NULL;
    }
}

// Swap bytes in ATA IDENTIFY model string (bytes are swapped per word)
static void fix_ata_string(char *s, int len) {
    for (int i = 0; i < len; i += 2) {
        char tmp = s[i];
        s[i] = s[i + 1];
        s[i + 1] = tmp;
    }
    // Trim trailing spaces
    for (int i = len - 1; i >= 0 && s[i] == ' '; i--)
        s[i] = '\0';
}

// Issue IDENTIFY DEVICE or IDENTIFY PACKET DEVICE
static int identify_device(uint8 port, uint8 type, AHCIDevice *dev) {
    uint64 buf_phys = alloc_pages(1);
    memset((void *)buf_phys, 0, 4096);

    uint8 cmd = (type == AHCI_DEV_SATAPI) ? ATA_CMD_IDENTIFY_PACKET : ATA_CMD_IDENTIFY;
    int rc = ata_command(port, cmd, 0, 0, buf_phys, 512, type == AHCI_DEV_SATAPI);
    if (rc != 0) return -1;

    uint16 *id = (uint16 *)buf_phys;

    // Extract model string (words 27-46, 40 chars)
    memcpy(dev->model, &id[27], 40);
    dev->model[40] = '\0';
    fix_ata_string(dev->model, 40);

    if (type == AHCI_DEV_SATA) {
        // Total sectors (LBA48): words 100-103
        dev->sector_count = *(uint64 *)&id[100];
        dev->sector_size = 512;
    } else {
        // ATAPI: sector count not in IDENTIFY; use capacity command later
        dev->sector_count = 0;
        dev->sector_size = 2048;
    }

    return 0;
}

void ahci_init(void) {
    const PCIDevice *pci = pci_find_class(0x01, 0x06);
    if (!pci) {
        kprint("AHCI: no SATA controller found\n");
        return;
    }
    if (!pci->bar[5] || !pci->bar_is_mmio[5]) {
        kprint("AHCI: BAR 5 not MMIO\n");
        return;
    }

    // Enable bus mastering and MMIO
    uint16 cmd = pci_config_read16(pci, 0x04);
    cmd |= PCI_COMMAND_BUS_MASTER | PCI_COMMAND_MMIO;
    pci_config_write16(pci, 0x04, cmd);

    // Map ABAR
    map_mmio_range(pci->bar[5], 0x10000);
    abar = (volatile uint8 *)pci->bar[5];

    // Enable AHCI mode
    hba_write(HBA_GHC, hba_read(HBA_GHC) | GHC_AE);

    uint32 pi = hba_read(HBA_PI);

    device_count = 0;
    for (uint8 i = 0; i < 32 && device_count < AHCI_MAX_PORTS; i++) {
        if (!(pi & (1u << i))) continue;

        uint8 type = detect_port_type(i);
        if (type == AHCI_DEV_NULL) continue;

        port_init_dma(i);

        AHCIDevice *dev = &devices[device_count];
        dev->type = type;
        dev->port_num = i;
        dev->present = 1;
        dev->model[0] = '\0';
        dev->sector_count = 0;
        dev->sector_size = (type == AHCI_DEV_SATAPI) ? 2048 : 512;

        if (identify_device(i, type, dev) == 0) {
            kprint("AHCI: port ");
            kprint_dec(i);
            kprint(type == AHCI_DEV_SATAPI ? " SATAPI " : " SATA ");
            kprint(dev->model);
            kprint("\n");
        } else {
            kprint("AHCI: port ");
            kprint_dec(i);
            kprint(type == AHCI_DEV_SATAPI ? " SATAPI" : " SATA");
            kprint(" (identify failed)\n");
        }

        device_count++;
    }

    kprint("AHCI: ");
    kprint_dec(device_count);
    kprint(" device(s) found\n");
}

uint32 ahci_device_count(void) {
    return device_count;
}

const AHCIDevice *ahci_get_device(uint32 idx) {
    if (idx >= device_count) return NULL;
    return &devices[idx];
}

int ahci_read_sectors(uint32 dev_idx, uint64 lba, uint32 count, void *buf) {
    if (dev_idx >= device_count) return -1;
    AHCIDevice *dev = &devices[dev_idx];
    if (!dev->present) return -1;

    uint32 buf_size = count * dev->sector_size;

    // Use a DMA buffer from high memory (must be physically contiguous)
    uint32 pages = (buf_size + 4095) / 4096;
    uint64 dma_buf = alloc_pages(pages);
    memset((void *)dma_buf, 0, pages * 4096);

    int rc;
    if (dev->type == AHCI_DEV_SATAPI) {
        rc = atapi_read(dev->port_num, lba, count, dma_buf, buf_size);
    } else {
        rc = ata_command(dev->port_num, ATA_CMD_READ_DMA_EXT,
                         lba, (uint16)count, dma_buf, buf_size, 0);
    }

    if (rc == 0) {
        memcpy(buf, (void *)dma_buf, buf_size);
    }

    // Note: dma_buf pages are leaked (no page free). Acceptable for
    // occasional reads; proper page freeing comes with a real allocator.
    return rc;
}
