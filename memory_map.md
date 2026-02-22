# Memory Map

## Layout

| Address | Size | Purpose | Status |
|---------|------|---------|--------|
| 0x0000-0x03FF | 1 KB | IVT | USED |
| 0x0400-0x04FF | 256 B | BIOS Data | USED |
| 0x0500-0x7BFF | 30 KB | - | FREE |
| 0x7C00-0x7DFF | 512 B | Bootsector | USED |
| 0x7E00-0x7FFF | 512 B | - | FREE |
| 0x8000-0x8FFF | 4 KB | Stack (16-bit) | USED |
| 0x9000-0xFFFF | 28 KB | - | FREE |
| 0x1000-~0x8000 | 27 KB | Kernel .text | USED |
| ~0x8000-~0xA000 | 8 KB | Kernel .data | USED |
| ~0xA000-~0xD000 | 12 KB | Kernel .bss | USED |
| ~0xD000-0x9FFFF | 588 KB | - | FREE |
| 0xA0000-0xBFFFF | 128 KB | VGA (0xB8000) | HW |
| 0xC0000-0xFFFFF | 256 KB | BIOS ROM | HW |
| 0x100000 | 4 KB | PML4T | USED |
| 0x101000+ | 5-20 MB | PDPT/PDT/PT | USED |
| PAGING_END+ | - | - | FREE |

## Constants

```
KERNEL_START    = 0x1000
MEMMAP_START    = 0x8000
PML4T_LOCATION  = 0x100000
```

## Free Zones

Low memory: 0xD000-0x9F000 (~588 KB)
High memory: PAGING_LOCATION_END → RAM_END

## Stack

| Mode | Address |
|------|---------|
| 16-bit | 0x8F000 |
| 32-bit | 0x9FFFF |
| 64-bit | 0x9FFFF |

## E820 Format

24 bytes per entry at MEMMAP_START:
- qword: base
- qword: length
- dword: type
- dword: ACPI flags
