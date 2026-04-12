# Changelog

## [2026-04-12] - NIC assignment modes and per-CPU thread metadata

### Added
- ThreadMeta struct in kernel/cpu.h: per-CPU snapshot of cpu_index, NUMA node, assigned NIC (PCI BDF + MAC). Will be mapped read-only into ring 3 thread address space when AP threads are implemented.
- NIC assignment modes: per-numa (one NIC per NUMA node, shared) and per-core (one NIC per core, locality-respecting)
- BSP_NIC_COUNT (=2) reserves the first 2 NICs in enumeration order for BSP use (mgmt + inter-node), excluded from the AP assignment pool
- nic_assign() recomputes the per-CPU assignment respecting NUMA locality strictly (no fallback to non-local NICs)
- Auto-detected default mode at boot: per-core if AP NICs >= AP cores, otherwise per-numa
- sys.nic.mode [per-numa|per-core] - show or set the assignment mode
- sys.thread.ls - per-CPU table with NUMA, NIC name, PCI BDF, MAC

## [2026-04-11] - PCI vendor and class name lookup

### Added
- scripts/gen_pci_ids.sh fetches pci.ids from pci-ids.ucw.cz and generates src/drivers/pci_ids.c
- Vendor whitelist (~25 entries) keeps the embedded table small
- pci_vendor_name() and pci_class_name() lookup functions
- sys.pci.ls now shows vendor and class names after the NUMA column

## [2026-04-11] - Pin MEMMAP_START at fixed address

### Changed
- MEMMAP_START moved from .bss to absolute address 0x500 (defined in link.ld)
- Removes 16-bit relocation overflow that prevented growing the kernel beyond ~64KB total
- The 1024-byte E820 buffer overlaps the relocated bootsector area at 0x600-0x7FF (unused after kernel takes over)

## [2026-04-11] - Bootsector chunked memcopy

### Changed
- CD-ROM memcopy path now uses NASM %rep to unroll 64KB chunks, advancing ES/DS between iterations
- Supports kernels of any size, not limited to 64KB by 16-bit DI/SI register width

## [2026-04-11] - AML subset walker for DSDT/SSDT

### Added
- src/arch/aml.c: ~400-line AML grammar walker for extracting Device(_BBN, _PXM) declarations from DSDT/SSDT
- Parses Scope, Device, Method, Name with PkgLength and NameString grammar
- Skips opcodes it doesn't understand using PkgLength when possible
- Handles Method(_PXM, 0) {Return(constant)} as well as Name(_PXM, X)
- Used as a fallback in acpi_pci_to_node() to discover PCI host bridge proximity from DSDT _PXM (which QEMU's pxb-pcie uses instead of SRAT Type 5)

## [2026-04-11] - PCI device NUMA proximity

### Added
- SRAT Type 5 (Generic Initiator Affinity) parsing in arch/acpi.c
- ACPINumaPCIAffinity struct and acpi_pci_to_node() with three lookup paths: SRAT Type 5, AML _BBN/_PXM walk, and ECAM base address inference from SRAT memory affinities
- numa_node field in PCIDevice, populated during enumeration
- numa_node field in NICSlot, copied from underlying PCIDevice
- nic_get_numa_node() and nic_find_for_node() in the NIC API
- sys.pci.ls and sys.nic.ls show NUMA proximity per device
- QEMU scripts use pxb-pcie + pcie-root-port to give each NIC a NUMA affinity

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
