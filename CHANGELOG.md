# Changelog

## [2026-02-22] - Boot Fixes

### Fixed
- Fixed K16 panic on boot: disabled interrupts until IVT setup complete
- Fixed triple fault on PAE enable: load CR3 before enabling PAE
- Fixed triple fault on paging enable: disable interrupts before enabling paging
- Removed scroll() call causing gibberish on screen
- Fixed reboot command: removed -no-reboot flag from QEMU, added multi-method reset

### Added
- Debug scripts: compile_qemu_debug.sh, check_qemu_log.sh, gdb_debug.sh
- Documentation: DEBUG.md, memory_map.md
- QEMU logging with -d int,cpu_reset,guest_errors

### Changed
- Reordered long mode init: CR3 → PAE → LME → Paging
- Reduced reboot wait loops from 10M to 100K iterations
- Split normal and debug QEMU launch scripts

## Boot Sequence
1. 16-bit real mode: load kernel, setup stack
2. Get memory map via E820
3. Setup IVT (interrupts disabled)
4. 32-bit protected mode: GDT, PIC, IDT, PIT
5. Create paging structures at 0x100000
6. Enable PAE, LME, paging
7. 64-bit long mode: IDT, memory manager, shell

## Memory Layout
- 0x00001000: Kernel (.text, .data, .bss)
- 0x00008000: Memory map (E820 entries)
- 0x0000D000-0x9F000: Available (~588 KB)
- 0x00100000: Paging structures (PML4T, PDPT, PD, PT)
- After paging end: Main free area
