# Memory Map

**Audience:** Kernel developers. This is the canonical reference for physical memory layout. For debugging procedures, see `DEBUG.md`. For architecture decisions, see `ARCHITECTURE.md`.

## Layout

| Address | Size | Purpose | Status |
|---------|------|---------|--------|
| 0x0000-0x03FF | 1 KB | IVT | USED |
| 0x0400-0x04FF | 256 B | BIOS Data | USED |
| 0x0500-0x05FF | 256 B | - | FREE |
| 0x0600-0x07FF | 512 B | Relocated Bootsector | USED |
| 0x0800-0x0FFF | 2 KB | - | FREE |
| 0x1000-~0x8000 | 28 KB | Kernel .text + .data | USED |
| 0x7C00-0x7DFF | 512 B | Bootsector (BIOS load, overwritten by kernel) | TEMP |
| ~0x8000-~0xA400 | ~9 KB | Kernel .bss (MEMMAP, IDT, BSP stack) | USED |
| ~0xA400-~0x4A400 | 256 KB | AP stacks (16 KB x 16 CPUs) | USED |
| 0xD000-0x9F000 | 584 KB | Heap | FREE |
| 0xA0000-0xBFFFF | 128 KB | VGA (text buffer at 0xB8000) | HW |
| 0xC0000-0xFFFFF | 256 KB | BIOS ROM / ACPI tables | HW |
| 0x100000 | 4 KB | PML4T | USED |
| 0x101000+ | 5-20 MB | PDPT/PDT/PT | USED |
| PAGING_END+ | - | - | FREE |
| 0xFEC00000-0xFEF00000 | 3 MB | APIC MMIO (IOAPIC + LAPIC, mapped at runtime) | HW |

### Runtime-only areas (used temporarily during AP startup)

| Address | Size | Purpose | Status |
|---------|------|---------|--------|
| 0x7000 | ~4 KB | AP trampoline temporary stack | TEMP |
| 0x8000-0x8FFF | 4 KB | AP trampoline code (copied at runtime) | TEMP |
| 0x8FF0-0x8FF7 | 8 B | AP trampoline: CR3 value | TEMP |
| 0x8FF8-0x8FFF | 8 B | AP trampoline: percpu base ptr | TEMP |

**Note**: The AP trampoline is copied to 0x8000 during SMP bringup. This overlaps with
the kernel .bss (MEMMAP_START). By that point, the memory map has already been parsed
and the data at MEMMAP_START is no longer needed.

## Constants

```
KERNEL_START    = 0x1000
BOOTSECT_RELOC  = 0x0600
MEMMAP_START    = 0x8000  (in .bss)
HEAP_START      = 0xD000
HEAP_END        = 0x9F000
PML4T_LOCATION  = 0x100000
LAPIC_BASE      = 0xFEE00000
IOAPIC_BASE     = 0xFEC00000
```

## Free Zones

Low memory: 0xD000-0x9F000 (~584 KB) - used as heap
High memory: PAGING_LOCATION_END - RAM_END

## Stack

| Mode | Address | Notes |
|------|---------|-------|
| 16-bit (bootsector) | SS=0x8000, SP=0xF000 = 0x8F000 | |
| 16-bit (kernel) | SS=0x8000, SP=0xF000 = 0x8F000 | |
| 32-bit | stack_end (~0xA400) | In .bss |
| 64-bit (BSP) | stack_end (~0xA400) | Same as 32-bit |
| 64-bit (APs) | ap_stacks + (cpu_idx+1)*16384 | 16 KB per AP |

## SMP / NUMA

- Up to 16 CPUs supported (MAX_CPUS)
- QEMU configured with 4 CPUs: 2 sockets x 2 cores
- NUMA node 0: CPUs 0-1 (1 GB)
- NUMA node 1: CPUs 2-3 (1 GB)
- APs are woken via INIT-SIPI-SIPI, park in hlt loop

## E820 Format

24 bytes per entry at MEMMAP_START:
- qword: base
- qword: length
- dword: type
- dword: ACPI flags
