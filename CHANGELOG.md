# Changelog

## [2026-04-11] - Reorganize src/ by subsystem

### Changed
- Move flat src/ layout into subdirectories: boot/, kernel/, arch/, drivers/, net/, fs/, shell/
- Drop the system. prefix from .c and .h filenames
- include/ mirrors the source tree (kernel/, arch/, drivers/, net/, fs/, shell/)
- Includes use the new paths: #include <kernel/mem.h>, #include <drivers/ide.h>, etc.
- Build output renamed from kernel to kernel.bin (to free up the kernel/ directory name)
- Makefile rewritten with subdirectory-aware rules

### Removed
- src/system.kbd.c (2-byte empty stub)

## [2026-04-10] - Renamed project from ZINC to Isurus

### Changed
- Project renamed to Isurus (shark genus containing the mako) to avoid collision with existing Zinc OS
- "Zero-Interrupt NUMA Cluster" kept as descriptive subtitle (no longer an acronym)
- ISO volume label: ZINC_OS -> ISURUS_OS

## [2026-04-02] - PCI enumeration, virtio-net driver, NIC abstraction

### Added
- PCI device enumeration via PCIe ECAM (MCFG ACPI table)
- Config space read/write, BAR decoding (MMIO/IO, 32/64-bit)
- Virtio PCI transport: capability walking, virtqueue allocation, feature negotiation
- Virtio-net driver: TX/RX via virtqueues, MAC address, link status
- NIC abstraction layer: vtable dispatch supporting multiple NIC drivers
- sys.pci.ls and sys.nic.ls shell commands
- Generic map_mmio_range() for arbitrary physical MMIO mapping
- memcpy/memset for freestanding kernel

### Changed
- QEMU machine type switched from i440fx to Q35 (PCIe ECAM support)
- All QEMU scripts include 4 virtio-net-pci devices (2 BSP + 1 per NUMA node)
- Linker script captures GCC subsections (.text.*, .rodata.*, .bss.*) and discards .eh_frame/.note.gnu.property
- IDE driver skips init when no controller present (Q35 has AHCI, not PIIX IDE)
- Boot init sequence: pci_init and nic_init called after fat32_init

## [2026-03-31] - Ring 3 shell with SYSCALL/SYSRET

### Added
- GDT64 expanded with ring 3 code/data segments (0x18, 0x20, 0x28) and TSS descriptor (0x30)
- Per-CPU TSS array with RSP0 for ring 3 to ring 0 transitions
- SYSCALL/SYSRET infrastructure: IA32_STAR, IA32_LSTAR, IA32_SFMASK MSRs, EFER.SCE
- SWAPGS-based per-CPU data access pattern (IA32_KERNEL_GS_BASE = &percpu[cpu])
- Syscall dispatch table with 23 syscalls (display, memory, disk, FS, ACPI, task management)
- User-side syscall wrappers in include/syscall.h (inline asm)
- Input ring buffer decoupling keyboard/serial ISRs from ring 3 shell
- BSP cooperative multitasking: task create, yield, exit, wait/wake, round-robin scheduling
- sys.cpu.ring shell command to verify current privilege level

### Changed
- Shell runs in ring 3 on BSP via IRETQ transition from kernel
- Shell uses syscall wrappers for all kernel services (putc, kprint, disk, memory, ACPI)
- Keyboard and serial drivers push to input ring buffer instead of calling shell directly
- PerCPU struct expanded to 40 bytes (added user_stack_top, user_rsp, cr3, in_usermode)
- AP trampoline updated for new PerCPU size

## [2026-03-27] - Move stacks to high memory

### Changed
- BSP stack (64KB) and AP stacks (16KB x CPUs) allocated from high memory (above 1MB) via PAGING_LOCATION_END bump allocator
- Heap range now dynamic: starts at page-aligned kernelEnd instead of hardcoded 0xD000
- BSS reduced from ~336KB to ~12KB (only MEMMAP, IDT, 4KB temp stack, C globals)

### Fixed
- BSS/heap overlap: BSP stack and AP stacks in BSS collided with heap at 0xD000-0x9F000

## [2026-03-27] - Boot Rework and SMP Fix

### Changed
- Bootsector rewritten: multi-path boot (LBA, CHS, CD-ROM no-emulation El Torito)
- KSIZE computed dynamically from kernel binary size
- Build switched to xorriso El Torito no-emul ISO as primary boot artifact
- QEMU launches from ISO with NUMA topology and serial trace
- 16-bit display routines rewritten with rep movsw/stosw
- AP trampoline moved from 0x8000 to 0x9F000 (was overwriting kernel .data)
- APs park with cli (BSP owns all interrupts)
- Trampoline uses segment-relative addressing (ORG 0, DS=CS) for high-memory compatibility
- memory_map.md rewritten with correct addresses and starts-at column

### Added
- Serial debug trace on COM1 for boot diagnostics (DBG macro)
- devbox.json/devbox.lock for reproducible build toolchain
- C23 bool/true/false guards in types.h

### Fixed
- AP trampoline at 0x8000 overwrote kernel .data, causing VGA garbage
- AP trampoline at 0x9F000 triple-faulted: 16-bit address overflow with ORG > 0xFFFF
- Explicit -fno-stack-protector (devbox GCC enables it by default, unusable without FS base)

## [2026-03-14] - Project Rename and Documentation Overhaul

### Changed
- Renamed project from "Remote NUMA Kernel" to ZINC (Zero-Interrupt NUMA Cluster)
- License changed from GPL-2.0 to dual AGPLv3 (community) / commercial
- Added status notices to README.md, ARCHITECTURE.md, application-model.md (target architecture vs current state)
- Marked SPDK as planned (not yet implemented) across all docs
- Standardized documentation audience pointers and cross-references
- Clarified DSM page fault mechanism (how BSP returns page data to faulting AP)
- Clarified locality map is intentionally static after failover (by design, not an accident)
- Updated ARCHITECTURE.md status section to include device libraries

### Added
- LICENSE file: dual AGPLv3 / commercial with support and contact info
- LICENSES/AGPL-3.0.txt: full AGPLv3 text
- Linux HPC comparison section in doc/user/application-model.md
- Documentation guide in README.md (per-audience reading paths)
- Glossary entries: ZINC, Affinity, Locality map, Ownership, Soft real-time, Tier 1/2/3
- Soft real-time characterization in application-model.md

### Removed
- doc/memory_use.text (stale draft, contradicted memory_map.md)
- doc/todo.text (most items already completed, replaced with new version)
- doc/docresources.text (bare URLs with no context)
- All GPU/GPDK references (removed from README, ARCHITECTURE, application-model, glossary)
- Duplicated boot sequence from CHANGELOG.md
- Duplicated memory layout from DEBUG.md (now references memory_map.md)

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
- Reordered long mode init: CR3 - PAE - LME - Paging
- Reduced reboot wait loops from 10M to 100K iterations
- Split normal and debug QEMU launch scripts

See `DEBUG.md` for boot sequence details and `memory_map.md` for memory layout.
