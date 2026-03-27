# Changelog

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
