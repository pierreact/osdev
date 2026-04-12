# Roadmap

## Done

D - Long to hex in C.
D - Rework IDT and ISRs, some trap and fault IDT entries have to be set properly.
D - Relocate some data structures in memory. The kernel is growing and getting over those.
D - If still present, fix the page fault bug.
D - Find last address of paging structure.
D - Find last address reachable.
D - Check paging structures from C.
D - Map remaining free addresses (Over 4GB).
D - Memory manager(/On screen Memory map?)
D - KB driver.
D - Disk Driver.
D - FS driver.
D - Kernel shell.
D - kmalloc, kfree.
D - IDE support.
D - FAT32 read-only driver (ls, cat).
D - 80x28 text mode, C monitor driver.
D - SMP: AP bringup (INIT-SIPI-SIPI, trampoline).
D - ACPI: MADT parsing for CPU/IOAPIC discovery.
D - Parse SRAT (System Resource Affinity Table) for automatic NUMA topology discovery.
D - Parse SLIT (System Locality Information Table) for latency distance matrix.
D - Parse MCFG for PCIe ECAM base address (needed to enumerate and configure NICs).
D - Parse HPET for high-precision timer (replace PIT, nanosecond resolution).
D - FACP: use PM timer for precise timing, use reset register for cleaner reboot.
D - DMAR/IVRS: IOMMU support for DMA protection and device isolation.
D - ISO-first boot workflow: xorriso El Torito no-emul ISO, QEMU boots from ISO, data disk optional.
D - Move shell from kernel to ring 3 on BSP (syscalls into ring 0 for kernel services).
D - GDT64 expansion: ring 3 code/data segments, TSS descriptor.
D - Per-CPU TSS with RSP0 for ring transitions.
D - SYSCALL/SYSRET via IA32_STAR/LSTAR/SFMASK MSRs, SWAPGS per-CPU pattern.
D - Syscall dispatch table (23 syscalls: display, memory, disk, FS, ACPI, task management).
D - Input ring buffer decoupling ISR from ring 3 shell.
D - BSP cooperative multitasking infrastructure (task create/yield/exit/wait/wake).
D - Namespaced commands (sys.cpu.ls, sys.numa.ls, sys.fs.df, sys.mem.free, etc.).
D - Tab completion walking the namespace tree.
D - Define Isurus syscall interface (syscall instruction, numbers, register convention).
D - PCI enumeration (ECAM via MCFG).
D - Virtio-net driver (virtqueue TX/RX, MAC, link status).
D - NIC abstraction layer (vtable dispatch, multiple driver support).
D - Q35 machine type for PCIe support.
D - IDE controller detection skip on Q35/AHCI.
D - PCI device NUMA proximity (SRAT Type 5 + AML _BBN/_PXM walker).
D - Subset AML walker (DSDT/SSDT) for Device(_BBN, _PXM) extraction.
D - Bootsector chunked memcopy (supports kernels >64KB).
D - Pin MEMMAP_START at fixed address 0x500 to free 16-bit relocation budget.
D - PCI vendor and class name lookup (embedded pci.ids subset).
D - NIC assignment modes: per-numa, per-core, with auto-detected default.
D - Per-CPU ThreadMeta struct with NUMA node and assigned NIC info.
D - sys.thread.ls and sys.nic.mode shell commands.

## Upcoming work

### Shell and userland
T - Filesystem layout: /bin (binaries), /home, /log, /conf (configurations).
T - Commands as separate binaries in /bin, loaded from FAT32.
T - Keymap loader from FS.
T - Telnet server on BSP management NIC (concurrent multi-user sessions).
T - Per-connection shell state (socket, input buffer, command history).
T - User identification (username prompt, no password, must match existing user in /conf).
T - root user exists by default, can create other users. No home directory for root.
T - Common users get a personal home directory in /home/<username>.

### Cross-compilation toolchain
T - GCC cross-toolchain target (x86_64-unknown-isurus): machine config, linker defaults.
T - Libc (malloc, printf, string operations, math, time functions) wrapping Isurus syscalls.
T - Default linker script for Isurus userland binaries.

### Future consideration
- Time-bound cluster utilisation per user. Policy-based scheduling: after a defined period, user A's jobs stop to let user B's jobs start. Users can pre-configure job definitions so the system starts them automatically when their time slot begins. Enables unattended time slicing of cluster resources.

### Kernel improvements
T - FAT32 write support (BSP system disk: /bin, /conf, /home, /log).
T - Slab allocator (replace O(n) bitmap scan for frequent small allocations).
T - Ring 3 thread execution on AP cores.
T - Per-thread page tables (CR3 per AP).
T - User program coordinator task on BSP (master for threads spanning APs).
T - Binary loader (load and execute /bin commands from FAT32).
T - Shared memory region mapping.
T - Locality map generation.

### Drivers
T - NVMe driver.
T - TCP/IP stack (userspace, runs in ring 3 polling loop).

### Device libraries
T - DPDK: userspace NIC library (DMA rings, RSS, per-thread queues, TCP/IP).
T - SPDK: userspace NVMe library (direct block access, polling).
T - Blobstore: block-level allocator over SPDK (named extents, no filesystem overhead).
T - BlobFS: minimal filesystem over Blobstore (flat namespace, file semantics for apps).

### Multi-node / clustering
T - Inter-node NIC driver (layer 2 communication between nodes).
T - DSM layer: page fault to network request path.
T - Coherence protocol: cache invalidation on write, coordinated through BSP.
T - Page replication to backup nodes.
T - Heartbeat-based failure detection.
T - Thread re-instantiation on failure.
T - Scheduler: one-time placement solver with affinity constraints.
T - Userspace timer API (HPET-based, nanosecond resolution).
T - Cross-node time synchronization.
T - PTP (Precision Time Protocol) for hardware-assisted clock synchronization across nodes.
