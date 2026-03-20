# Architecture Decisions

> **Note:** This document describes the target architecture. The kernel currently runs on a single node. Multi-node clustering, DSM, DPDK, and SPDK are not yet implemented. Sections below describe the intended design, not the current state.

**Audience:** Kernel developers and contributors. For application-level guidance, see `doc/user/application-model.md`. For research context, see `README.md`.

Decisions and rationale behind kernel design choices.

Performance is the only driver for this project.
It's meant to live in an isolated environment.

## Memory allocation

malloc allocates a minimum of 4KB (one page). All allocations are multiples of 4KB.

- No sub-page allocation. This eliminates internal fragmentation tracking and the overhead of splitting/coalescing free blocks.
- The allocator is O(n) bitmap scan with zero metadata per allocation, this need to be improved, maybe removing malloc and giving a (MEM/Cores)-shared statically.
- Fragmentation is a performance problem we avoid entirely by not allowing it.

Trade-off: wastes memory on small allocations. Acceptable for a kernel where most allocations are page-sized anyway (page tables, buffers, per-CPU structures).

## Execution model

- BSP (CPU 0) runs the kernel and management services.
- BSP has 2 NICs:
  - Inter-node NIC: dedicated for cluster communication between nodes (layer 2).
  - Management NIC: full TCP/IP stack for SSH, telnet, and maintenance.
- APs (all other cores): each core runs one user thread in ring 3.
- Ring 3 provides memory isolation. Threads cannot corrupt kernel memory or each other (see Thread memory isolation).
- Device MMIO (NICs, storage controllers) is mapped into each thread's address space by the kernel via page tables. Threads access devices directly via polling without syscalls (see Device assignment).

## Device assignment

Devices (NICs, storage controllers) are assigned either per NUMA node or per core, at the user's discretion. Both modes are supported:

- **Per NUMA node:** one device shared by all cores on that node via hardware queues (e.g. RSS/VMDq for NICs). No locking between threads.
- **Per core:** one device per thread. No sharing, maximum throughput.

This applies uniformly to NICs (DPDK) and storage controllers (SPDK, planned).

BSP has 2 additional NICs (inter-node + management). These are separate from the application devices.

All devices are accessed via polling from userspace (ring 3). No interrupts. BSP handles all interrupts in the system. Device MMIO is mapped into the thread's address space by the kernel.

DMA buffers, driver state, and the thread touching the data are all on the same NUMA node. Zero copy. No cross-node memory access for I/O.

## ACPI

- FACP power saving
  - C states: We won't go deeper than C1 (hlt).
  - P states: We'll keep the CPU at max frequency.
  - T states: We'll ignore them, just cool your CPU appropriately.
- ACPI root table discovery supports RSDT (ACPI 1.0) and XSDT (ACPI 2.0+), with checksum validation.
- Parsed tables:
  - MADT (CPU/LAPIC/IOAPIC/ISA overrides)
  - SRAT (CPU and memory NUMA affinity)
  - SLIT (NUMA distance matrix)
  - HPET (timer MMIO metadata)
  - FADT/FACP (PM timer and reset register)
  - MCFG (PCIe ECAM segments)
  - DMAR / IVRS (IOMMU unit discovery)
- Exposed through ACPI query functions so shell/scheduler/memory/PCI/reboot code can consume parsed data without reading raw ACPI structures directly.

## Device libraries

The OS provides userspace libraries for direct device access from ring 3:

- **DPDK** (NICs): DMA ring management, packet buffer allocation (NUMA-local), TX/RX descriptors, per-thread hardware queue assignment. Full TCP/IP available through the library.
- **SPDK** (storage controllers, planned): direct storage access, following the same pattern as DPDK.

Both follow the same design: kernel maps the device MMIO, userspace library drives it via polling. All memory given to the libraries is NUMA-local to the thread.

In Linux, DPDK is a workaround against the OS: it bypasses the kernel to poll the NIC from userspace. Here, direct device access is the intended design. The kernel maps the device and provides the library. There is nothing to bypass.

## Thread-core binding

- Threads run in ring 3. One thread per core. Pinned at creation, never migrated during normal operation.
- Device access through mapped MMIO, not syscalls. Polling, not interrupts.
- A thread's core determines its NUMA node. Its stack, heap allocations, and working set are all local to that NUMA node.
- There is no migration and no load balancing. The scheduler is a one-time placement solver (see README section 7). It runs again only on node failure to reschedule dead threads to available cores.

## Thread memory isolation

Each AP loads its own CR3, pointing to its own PML4T. This gives each thread a fully independent address space: different virtual-to-physical mappings, different permission bits, complete isolation. Intel MPK (Skylake+) can toggle read/write permissions per-thread within a shared address space, but is limited to 16 protection keys and does not provide address space separation. Per-thread CR3 is the stronger mechanism and the one ZINC uses.

One thread per core, no context switching. CR3 is loaded once during AP startup and never changes.

Three virtual address zones per thread:

```
Virtual address space (per thread):
+---------------------------+
| Kernel (supervisor)       |  Identical in all page tables. Same physical pages.
|                           |  Thread cannot access (ring 3 vs ring 0).
+---------------------------+
| Thread-private (user)     |  Only mapped in THIS thread's page table.
|  - Stack                  |  Physically allocated on thread's NUMA node.
|  - Private heap           |
|  - Device MMIO            |
+---------------------------+
| Shared region (user)      |  Mapped in ALL thread page tables at same VA.
|  - Cross-thread data      |  Physical pages may span multiple NUMA nodes.
|  - Coordination structs   |
+---------------------------+
```

Kernel and shared regions reuse the same physical page table pages across all PML4Ts. Only the private region's page table entries differ per thread. This keeps memory overhead small: ~16KB per thread (PML4T + private region page tables).

Private memory includes the thread's stack, local heap, and device MMIO (NICs, storage controllers assigned to the thread). All private allocations are physically on the thread's NUMA node.

Shared memory is for cross-thread data and coordination structures. Each shared page has a single writer; other threads read. Pre-partitioned at boot: each thread gets a fixed shared slice (inbox/outbox pattern). BSP-managed allocation is available for the rare case where dynamic shared memory is needed (thread requests allocation via syscall, BSP allocates, no lock contention because BSP is single-threaded).

### Locality map

A read-only page at a fixed virtual address, mapped into every thread's page table at boot. It tells each thread the locality tier of every shared slice:

- **Tier 1: same NUMA node.** Local memory read. Cheapest.
- **Tier 2: same machine, different NUMA node.** Cross-NUMA read. Hardware cache coherence applies. No network.
- **Tier 3: remote machine.** First access triggers a DSM page fault through BSP. Subsequent reads are cached until invalidated.

The map is static by design. It reflects initial placement and is intentionally not updated on failover. After a node failure, the map may be stale (a tier 1 slice may now be tier 3 because the writer was rescheduled elsewhere). This is a deliberate trade-off: updating the map would require synchronizing all threads cluster-wide, adding complexity to an already degraded state. Correctness is preserved (addresses still work), only performance degrades. See Thread re-instantiation for details.

## Shared memory and DSM

### Local shared memory

Local shared pages (same machine) are mapped at boot into all thread page tables at the same virtual address. Mappings are static; no TLB shootdowns needed. Hardware cache coherence handles consistency automatically within a single machine.

### Remote shared memory (cross-machine DSM)

Remote shared pages are fetched on demand via page faults. All DSM traffic goes through BSP's inter-node NIC (layer 2). The per-NUMA-node NICs are mapped into ring 3 for DPDK and cannot be used by the kernel.

1. Remote pages are initially unmapped (not present) in the local page tables.
2. When a thread accesses a remote page, a page fault fires (ring 0 on the faulting AP core).
3. The AP's fault handler sends an IPI to BSP with the faulting address.
4. BSP sends the request to the owning node over the inter-node NIC.
5. The owning node sends back the 4KB page contents to BSP.
6. BSP copies the page data into a pre-allocated frame on the faulting AP's NUMA node, updates the AP's page table to map it read-only, and sends an IPI to wake the AP.
7. The AP resumes the thread. The faulting instruction retries and succeeds.
8. The page is now cached locally. Writes require the coherence protocol (see below).

BSP serializes all DSM requests for the machine. This is acceptable because remote faults should be rare by design (data is partitioned, working sets are local). DSM fault frequency is exported via the Prometheus exporter for monitoring.

Why this is different from historical DSM systems (Treadmarks, Munin, Ivy):

- The kernel IS the DSM layer. Page fault to network request is a straight code path with no context switches, no syscall overhead.
- Layer 2 transport. No TCP/IP stack in the data path.
- NUMA-aware thread placement minimizes remote faults. Data can be partitioned so each thread's working set is mostly local.

Mitigation: align each thread's shared slice to page boundaries so a single page is never split across two writers' slices.

## Coherence protocol

Single-writer / multiple-reader. Ownership is permanent; each shared slice belongs to the thread that created it and never changes.

Writers write to their own slice. Readers on remote nodes fetch pages on demand (see Remote shared memory above). Fetched pages are cached read-only. When the writer modifies a page, the coherence protocol invalidates all cached copies:

1. Writer thread on node A modifies page P.
2. Node A sends INVALIDATE(P) to all nodes holding cached read-only copies.
3. Remote nodes unmap page P from their local page tables and ACK.
4. Next time a reader on a remote node accesses page P, it faults and fetches the updated copy.

BSP tracks which nodes hold cached copies of which pages. Invalidation is coordinated through BSP.

Another option would be to directly send the invalidated page to replace. This should be proposed as an option.

## Page replication for fault tolerance

All shared pages are replicated to a backup node on a different physical machine. Private thread memory is not replicated (thread is re-instantiated fresh on failure).

Write path: the writer's node sends the update to the backup synchronously. This guarantees the backup is never behind. Trade-off: every write to a shared page pays one extra network round trip. Acceptable for write-rare shared data (the common pattern in this architecture).

Backup selection: different physical machine, ideally latency-close to minimize replication latency.

Failover on node failure:

1. BSP detects dead node via heartbeat timeout.
2. BSP promotes the backup for all shared pages whose writer was on the dead node.
3. BSP broadcasts the updated page location map to the cluster.
4. Threads holding read-only cached copies of dead node's pages continue reading (data is still valid from the backup).
5. Threads that fault on pages from the dead node are redirected to the backup.

## Thread health and fault isolation

- Threads must heartbeat: periodically write to a designated memory area.
- If a thread stops updating its heartbeat, it is considered dead.
- Dead thread is killed, its NIC queue is reset, and the thread is re-instantiated.
- Fault isolation goal: a misbehaving thread should not kill the whole process across the cluster. At worst, kill the thread. Possibly kill the node, but never the entire cluster process.
- Implementation approach TBD. Areas to explore: page-level protection, NIC queue isolation, watchdog on BSP.
- When a node fails, BSP promotes backup pages to primary ownership (see Page replication for fault tolerance).
- Threads holding read-only cached copies of the dead node's pages continue reading; the data is still valid since the dead thread was the only one able to write there.
- About dead thread's shared write slot, still have to determine if:
  - Dead thread's shared write slot is untrusted until BSP scrubs it. Other threads must not read from a slot whose owner is dead (single-writer principle: the writer is gone, so the data may be partial).
  - Shared memory is not scrubed and new instance of thread restarts with it.

### Thread re-instantiation

When a single thread fails, BSP re-instantiates it on the same core. The thread restarts its task from scratch with fresh private memory. Shared page ownership is restored from backups if needed.

When an entire node fails, all threads on that node must be rescheduled. BSP places them on available cores elsewhere in the cluster, respecting NUMA affinity rules (threads that were co-located on the same NUMA node should remain co-located if possible). If no cores are available, tasks are queued until cores are freed.

A rescheduled thread keeps its original shared memory mappings at the same virtual addresses. It does not know it has moved. However, shared pages that were tier 1 (same NUMA node) or tier 2 (same machine) at the original placement may now be tier 2 or tier 3 from the new location. The thread silently pays higher latency for those accesses. The locality map is not updated; it still reflects the original placement. This is a trade-off: correctness is preserved (the addresses still work), but performance degrades. This is accepted because failover is a catastrophic event, not normal operation.

## Cluster memory model

A cluster is modeled as additional NUMA nodes with higher latency. From the scheduler's perspective, a remote machine is just another NUMA domain - same abstraction, different latency numbers.

- Inter-node communication goes over the dedicated NIC on BSP (layer 2).
- Management traffic goes over the separate management NIC on BSP (TCP/IP).

Memory on remote nodes is accessible but expensive. The allocator prefers local memory. Remote allocation requires explicit intent. Coherence across machines is handled by the DSM layer: remote pages are fetched on demand via page faults. Each shared slice has a permanent writer; readers fetch and cache pages read-only (see Coherence protocol).

**Current status:** the kernel runs on a single node. The cluster memory model, DSM layer, coherence protocol, inter-node communication, and userspace device libraries (DPDK, SPDK) described above are the target architecture and are not yet implemented.

## Libc

ZINC ships a minimal libc providing malloc, printf, string operations, math, time and helper functions for memory areas mentioned aboved. No POSIX. No dynamic linking; all binaries are statically linked. There is no dynamic linker.

## Timer architecture

ZINC uses the TSC (Time Stamp Counter) for nanosecond-resolution timing. RDTSC/RDTSCP is the primary runtime clock source on AP threads. ACPI HPET and FADT PM timer are parsed and exposed for calibration/validation and platform timing metadata. ACPI reset register (FADT) is used as the preferred reboot path when available, with existing fallback reset methods preserved.

Cross-node time synchronization through PTP will provide consistent timestamps across all machines in the cluster, enabling coordinated timing for distributed workloads.
