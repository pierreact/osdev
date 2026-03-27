# Memory Map

**Audience:** Kernel developers. This is the canonical reference for physical memory layout. For debugging procedures, see `DEBUG.md`. For architecture decisions, see `ARCHITECTURE.md`.

## Layout

All addresses below are approximate (~) where they depend on kernel binary size. Exact values shift as code grows. The kernel binary is currently ~40 KB (80 sectors).

| Address | Starts at | Size | Purpose | Status |
|---------|-----------|------|---------|--------|
| 0x0000-0x03FF | 0 KB | 1 KB | IVT (unused after boot, preserved) | USED |
| 0x0400-0x04FF | 1 KB | 256 B | BIOS Data (unused after boot) | USED |
| 0x0500-0x05FF | 1.25 KB | 256 B | - | FREE |
| 0x0600-0x07FF | 1.5 KB | 512 B | Relocated Bootsector (unused after boot) | FREE |
| 0x1000-~0xAFFF | 4 KB | ~40 KB | Kernel .text + .data (binary) | USED |
| 0x7C00-0x7DFF | 31 KB | 512 B | Bootsector (BIOS load, overwritten by kernel) | TEMP |
| ~0xB000 | ~44 KB | 1 KB | MEMMAP_START (E820 memory map, in .bss) | USED |
| ~0xB400 | ~45 KB | 4 KB | IDT (IDT64_BASE, in .bss) | USED |
| ~0xC400-~0x1C400 | ~49 KB | 64 KB | BSP stack (in .bss, grows down from stack_end) | USED |
| ~0x1C400-~0x5C400 | ~113 KB | 256 KB | AP stacks (16 KB x 16 CPUs, in .bss) | USED |
| 0xD000-0x9F000 | 52 KB | 584 KB | Heap | FREE |
| 0x9F000-0x9FFFF | 636 KB | 4 KB | AP trampoline (copied at runtime) | TEMP |
| 0xA0000-0xBFFFF | 640 KB | 128 KB | VGA (text buffer at 0xB8000) | HW |
| 0xC0000-0xFFFFF | 768 KB | 256 KB | BIOS ROM / ACPI tables | HW |
| 0x100000 | 1 MB | 4 KB | PML4T | USED |
| 0x101000+ | 1 MB + 4 KB | 5-20 MB | PDPT/PDT/PT | USED |
| PAGING_END+ | ~6-21 MB | - | - | FREE |
| 0xFEC00000-0xFEF00000 | 4076 MB | 3 MB | APIC MMIO (IOAPIC + LAPIC, mapped at runtime) | HW |

**WARNING: BSS/heap overlap.** The BSP stack (~0xC400-~0x1C400) and AP stacks (~0x1C400-~0x5C400) are in .bss and overlap with the heap range (0xD000-0x9F000). This is a latent bug: heap allocations return addresses inside the BSP/AP stack memory. The heap range or BSS layout needs to be fixed so they don't collide.

### Runtime-only areas (used temporarily during AP startup)

| Address | Size | Purpose | Status |
|---------|------|---------|--------|
| 0x9F000-0x9FFFF | 4 KB | AP trampoline code + data page | TEMP |
| 0x9FFF0-0x9FFF7 | 8 B | AP trampoline: CR3 value (patched by BSP) | TEMP |
| 0x9FFF8-0x9FFFF | 8 B | AP trampoline: percpu base ptr (patched by BSP) | TEMP |
| 0x7000 | ~4 KB | AP trampoline temporary stack | TEMP |

## Constants

```
KERNEL_START    = 0x1000
BOOTSECT_RELOC  = 0x0600
TRAMPOLINE_BASE = 0x9F000
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
| 32-bit | stack_end (~0x1C400) | In .bss |
| 64-bit (BSP) | stack_end (~0x1C400) | Same as 32-bit, 64 KB |
| 64-bit (APs) | percpu[i].stack_top | 16 KB per AP, in .bss |

## SMP / NUMA

- Up to 16 CPUs supported (MAX_CPUS)
- QEMU configured with 4 CPUs: 2 sockets x 2 cores
- NUMA node 0: CPUs 0-1 (1 GB)
- NUMA node 1: CPUs 2-3 (1 GB)
- APs are woken via INIT-SIPI-SIPI to trampoline at 0x0000
- APs park with interrupts disabled (cli; hlt) - BSP owns all IRQs

## E820 Format

24 bytes per entry at MEMMAP_START:
- qword: base
- qword: length
- dword: type
- dword: ACPI flags
