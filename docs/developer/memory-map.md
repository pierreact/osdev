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
| ~0xC400 | ~49 KB | 4 KB | Temp stack (32-bit and early 64-bit, in .bss) | USED |
| ~0xD400-~0x9F000 | ~53 KB | ~580 KB | Heap (dynamic: kernelEnd to 0x9F000) | FREE |
| 0x9F000-0x9FFFF | 636 KB | 4 KB | AP trampoline (copied at runtime) | TEMP |
| 0xA0000-0xBFFFF | 640 KB | 128 KB | VGA (text buffer at 0xB8000) | HW |
| 0xC0000-0xFFFFF | 768 KB | 256 KB | BIOS ROM / ACPI tables | HW |
| 0x100000 | 1 MB | 4 KB | PML4T | USED |
| 0x101000+ | 1 MB + 4 KB | 5-20 MB | PDPT/PDT/PT | USED |
| PAGING_END+ | ~6-21 MB | 64 KB | BSP stack (allocated at runtime) | USED |
| (after BSP stack) | ~6-21 MB | 16 KB each | AP stacks (allocated per CPU at runtime) | USED |
| (after AP stacks) | - | 104 B x 16 | TSS array (per-CPU, in BSS) | USED |
| (after TSS) | - | 64 KB each | BSP task kernel stacks (allocated per task) | USED |
| (after kstacks) | - | 64 KB each | BSP task user stacks (allocated per task) | USED |
| (after stacks) | - | - | - | FREE |
| 0xFEC00000-0xFEF00000 | 4076 MB | 3 MB | APIC MMIO (IOAPIC + LAPIC, mapped at runtime) | HW |

### Runtime-only areas (used temporarily during AP startup)

| Address | Size | Purpose | Status |
|---------|------|---------|--------|
| 0x9F000-0x9FFFF | 4 KB | AP trampoline code + data page | TEMP |
| 0x9FFF0-0x9FFF7 | 8 B | AP trampoline: CR3 value (patched by BSP) | TEMP |
| 0x9FFF8-0x9FFFF | 8 B | AP trampoline: percpu base ptr (patched by BSP) | TEMP |

## Constants

```
KERNEL_START    = 0x1000
BOOTSECT_RELOC  = 0x0600
TRAMPOLINE_BASE = 0x9F000
HEAP_START      = kernelEnd (dynamic, page-aligned)
HEAP_END        = 0x9F000
PML4T_LOCATION  = 0x100000
LAPIC_BASE      = 0xFEE00000
IOAPIC_BASE     = 0xFEC00000
```

## Free Zones

Low memory: kernelEnd to 0x9F000 (~580 KB) - used as heap
High memory: after stacks (PAGING_LOCATION_END+) to RAM_END

## Stack

| Mode | Address | Notes |
|------|---------|-------|
| 16-bit (bootsector) | SS=0x8000, SP=0xF000 = 0x8F000 | |
| 16-bit (kernel) | SS=0x8000, SP=0xF000 = 0x8F000 | |
| 32-bit | temp_stack_end (~0xD400) | 4KB temp stack in .bss |
| 64-bit (BSP early) | temp_stack_end (~0xD400) | Until alloc_bsp_stack runs |
| 64-bit (BSP) | PAGING_LOCATION_END+ | 64 KB, allocated from high memory |
| 64-bit (APs) | percpu[i].stack_top | 16 KB per AP, allocated from high memory |
| 64-bit (BSP ring 3) | task.user_rsp | 64 KB per task, allocated from high memory |
| 64-bit (BSP ring 0, syscall) | task.kstack_top | 64 KB per task kernel stack |

## SMP / NUMA

- Up to 16 CPUs supported (MAX_CPUS)
- QEMU configured with 4 CPUs: 2 sockets x 2 cores
- NUMA node 0: CPUs 0-1 (1 GB)
- NUMA node 1: CPUs 2-3 (1 GB)
- APs are woken via INIT-SIPI-SIPI to trampoline at 0x9F000
- APs park with interrupts disabled (cli; hlt) - BSP owns all IRQs

## E820 Format

24 bytes per entry at MEMMAP_START:
- qword: base
- qword: length
- dword: type
- dword: ACPI flags
