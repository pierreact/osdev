# Writing Applications for Isurus

> **Note:** This document describes the target architecture. The kernel currently runs on a single node. The BSP shell runs in ring 3 with SYSCALL/SYSRET. AP ring 3 threads, multi-node features (DSM, cross-machine memory, thread placement across machines, DPDK, SPDK) are not yet implemented.

**Audience:** Application developers writing code against Isurus. For kernel design rationale, see [Architecture](../developer/architecture.md). For research context and project overview, see [Research Overview](../research/overview.md).

This document describes the execution and memory model from the perspective of an application developer.

Current deployment model is ISO-first: machines boot the OS from a BIOS-bootable ISO built with **El Torito hard-disk emulation** of the full **`os.bin`** image (see `compile.sh` / `xorriso`). The primary boot path is **firmware + El Torito**, not GRUB loading the kernel file. Application/runtime data disks are separate and optional at boot. This keeps OS/tooling versions immutable and makes rollback a media switch (or PXE) instead of an in-place mutation.

Isurus turns off-the-shelf x86 hardware into a unified supercomputer. It runs on a single machine or across a cluster; commodity servers connected by standard networking, treated as one shared-memory machine with a single process, a single shared address space, spanning as many nodes as you plug in but still with some protection.

## Supported languages

Isurus provides a libc (malloc, printf, string operations, math, time functions) but not a full POSIX runtime. Any language that compiles to native code and can operate freestanding or link against a minimal libc is a candidate.

**Native, no runtime required:**
- **C:** the primary language. Direct hardware control, freestanding.
- **C++:** freestanding mode (`-ffreestanding`). Zero-cost abstractions via templates. Widely used in HPC.
- **Fortran:** dominant in scientific HPC (physics simulations, linear algebra). GCC's gfortran can cross-compile freestanding.
- **Zig:** designed as a "better C." No hidden allocations, no hidden control flow, freestanding by default.
- **Rust:** `no_std` mode, no runtime, memory safety at compile time.
- **D:** `betterC` mode strips the runtime entirely. Compiles to native code via GCC or LLVM backend.

**Not currently compatible:** Languages that require an interpreter (Python, Ruby, JavaScript) or a managed runtime with OS dependencies (Go, Java, C#, Erlang, Haskell). These runtimes depend on POSIX services that Isurus does not provide. Porting such a runtime to Isurus is theoretically possible but would be a significant effort.

## Execution model

Your application runs as a single process spanning one or more machines in the cluster. Each thread runs on exactly one core and never moves.

- One thread per core on APs. No context switching. Your thread owns the core entirely.
- BSP (CPU 0) runs the kernel and multiple ring 3 management tasks (shell, user program coordinator, telnet, etc.) via cooperative multitasking. BSP tasks yield explicitly; this is not preemptive.
- BSP handles all interrupts. Your thread is never interrupted. The only exception is a DSM page fault when accessing remote shared memory (see below).
- All CPUs run at maximum frequency at all times. No frequency scaling, no deep sleep states. When work arrives, it is processed immediately; no wake-up or ramp-up latency.
- Each thread polls its NIC directly (DPDK-style). No syscalls for network I/O.
- Each thread polls its Storage directly (SPDK-style). No syscalls for disk I/O.


### Real-time characteristics

Isurus provides no preemption, no scheduling jitter, no OS-generated interrupt latency on application threads, and deterministic core ownership with CPUs always at maximum capacity.

This benefits two categories of workloads. Latency-sensitive work (packet processing, storage I/O, real-time simulation, high-frequency trading) gets predictable response times. Throughput-oriented work (heavy computation, batch processing, streaming) gets every CPU cycle; no overhead lost to the OS.

**Single node: low-jitter.** All memory accesses are local (same NUMA node or cross-NUMA within the machine). No network, no DSM faults, no OS-generated interrupts on application cores, no scheduling. The remaining jitter sources are hardware-level: SMIs (firmware, invisible to any OS, can steal hundreds of microseconds), NMIs, and MCEs. These are infrequent but cannot be eliminated without firmware cooperation. Isurus gets you as close to hard real-time as x86-64 allows without custom firmware.

**Cluster: soft real-time.** DSM page faults introduce network-dependent latency when a thread first accesses remote shared memory. Subsequent accesses to the same page are cached and local. The latency bound depends on network conditions and BSP queue depth, not on a guaranteed worst-case deadline.

## Thread creation and placement

When you create threads, you specify placement constraints. The kernel's scheduler assigns each thread to a core before execution begins. Once placed, threads do not move.

### Affinity levels

Three levels of locality determine where threads can be placed relative to each other:

**Same NUMA node.** Cheapest communication. Threads on the same NUMA node read each other's shared memory slices via local memory access. Use this for threads that share data heavily (e.g. connection tracking table shared between packet-processing threads).

**Same machine, different NUMA node.** Cross-NUMA latency (higher than local, but no network). Hardware cache coherence still works. Use this for threads that communicate occasionally but do not need the lowest latency.

**Different machine.** Cross-machine reads go through the DSM layer; a page fault triggers a network request over the inter-node NIC. Use this for threads with large independent working sets that rarely read each other's state.

### Anti-affinity

You can also request that threads be separated:

- **NUMA anti-affinity:** spread threads across NUMA nodes (so each gets dedicated NIC, if you picked one NIC per NUMA for example).
- **Machine anti-affinity:** spread threads across machines so a single node failure does not kill all of them.

### Example

A stateful firewall with 8 packet-processing threads and a shared connection table:

- Place all 8 threads on the same NUMA node (NUMA affinity) so connection table reads are local.
- If 8 cores are not available on one node, use machine affinity (same machine, different NUMA nodes). Cross-NUMA reads are more expensive but avoid network round trips.
- For fault tolerance, split into two groups of 4 on different machines (machine anti-affinity). Accept that some connection lookups will go through DSM.

## Memory model

Each thread has three memory regions:

### Private memory

Your thread's stack, local heap, and working set. Only your thread can access this memory. It is physically allocated on your thread's NUMA node.

No other thread can read or write your private memory. This is enforced by hardware (each thread has its own page table).

If your thread dies, its private memory is reclaimed. The thread is re-instantiated fresh; there is no resume from where it left off at this layer, it's your code that deals with it.

### Shared memory

A single logical shared address space visible to all threads across all nodes. Each thread gets a pre-allocated slice of the shared region at boot.

**Single-writer principle:** each shared slice has exactly one writer (the thread that owns it). All other threads can read it. You write to your own slice. You read from others.

### Locality map

At a fixed virtual address in your thread's address space, the kernel provides a read-only locality map. It tells you the cost tier of every shared slice:

- **Tier 1: same NUMA node.** Local memory read. Cheapest.
- **Tier 2: same machine, different NUMA node.** Cross-NUMA read. No network involved.
- **Tier 3: remote machine.** First access triggers a DSM page fault. Subsequent reads are cached.

Use the locality map to make smart decisions: batch tier 3 reads, prefer tier 1 data when multiple copies exist, co-locate writers with their most frequent readers via affinity constraints.

On current kernel builds, NUMA locality metadata is sourced from ACPI SRAT (node affinity) and SLIT (distance matrix) when firmware provides those tables.

The map is static (set at boot, not updated on failover). After a node failure, tier information may be stale.

This means:
- No write contention. Each location has one writer.
- Readers must handle stale data. If you read another thread's slice, the writer may be mid-update. Use sequence counters or similar mechanisms to detect partial writes.

#### Local shared memory

If the writer thread is on the same machine, the read is a normal memory access. Hardware cache coherence handles consistency. This is fast; comparable to reading any memory on your NUMA node (or cross-NUMA if the writer is on a different node within the same machine).

#### Remote shared memory (DSM)

If the writer thread is on a different machine, the first read triggers a page fault. The faulting AP sends an IPI to BSP, which fetches the page from the owning machine over its inter-node NIC (layer 2, no TCP/IP overhead). The page is then cached locally as read-only.

Subsequent reads of the same page are local (cache hit). The page stays cached until the owner modifies it and the coherence protocol invalidates your copy.

You do not need to do anything special to read remote memory. A pointer dereference works. The DSM layer is transparent; the page fault and network fetch happen behind the scenes.

### Shared allocator

Most shared memory is pre-partitioned at boot. Each thread gets a fixed slice.

For dynamic shared allocation (rare), your thread requests memory from BSP via syscall. BSP allocates from the shared region and maps it into all thread page tables. This adds latency (syscall + IPI round trip) but avoids lock-free allocator complexity.

## Device access

Devices (NICs, storage controllers) are assigned per NUMA node or per core. The mode is auto-selected at boot from the resource counts (per-core when there are enough NICs for every core, otherwise per-numa) and can be overridden at runtime via `sys.nic.mode per-core|per-numa`.

- **Per NUMA node:** one device shared by all threads on that node via hardware queues (e.g. RSS/VMDq for NICs). No locking between threads.
- **Per core:** one device per thread. No sharing, so less induced latency and maximum throughput.

The first 2 NICs (in PCI enumeration order) are reserved as BSP NICs (management + inter-node) and excluded from the AP assignment pool. Both modes respect locality strictly: a thread on NUMA node N only ever gets a NIC on NUMA node N. If no matching NIC is available, the thread has no NIC.

### Thread metadata (ThreadMeta)

Each thread can read its own `ThreadMeta` to learn its placement:

- `cpu_index` — which core it runs on
- `numa_node` — which NUMA node that core belongs to
- `nic_index`, `nic_segment`/`nic_bus`/`nic_dev`/`nic_func` — the assigned NIC's PCI address
- `nic_mac[6]` — the MAC address of the assigned NIC

When ring 3 AP threads are implemented, this struct will be mapped read-only into each thread's address space at a fixed virtual address. Currently inspectable from the BSP shell via `sys.thread.ls`.

### Polling and zero-copy

All devices are accessed via polling from userspace. No interrupts, no syscalls. The kernel maps device MMIO into your thread's address space. You drive the device through the provided libraries:

- **DPDK** for NICs: DMA ring management, packet buffer allocation, TX/RX descriptors, per-thread hardware queue assignment. Full TCP/IP available through the library.
- **SPDK** for storage controllers (planned): direct storage access, same polling pattern.

All buffers are allocated on your thread's NUMA node. Zero copy; data is not moved between buffers.

## What Isurus does not provide

- **No dynamic linking.** All binaries are statically linked.
- **No virtual memory tricks.** Isurus uses virtual memory (page tables, per-thread CR3) for isolation and mapping, but no swap, no fork, no copy-on-write, no demand paging. Every page is backed by a physical frame, NUMA-local, allocated at thread creation.
- **No mmap.** Device MMIO is mapped by the kernel at thread setup. Storage is accessed directly via SPDK (planned), not through a filesystem. Shared memory is pre-partitioned or BSP-allocated. There is nothing left for mmap to do.
- **No scheduling on APs.** One thread per core, pinned at creation. No preemption, no context switching, no migration, no load balancing. BSP uses cooperative multitasking for management tasks (shell, coordinator, telnet), but this does not affect application threads.
- **No power management.** All CPUs run at maximum frequency at all times. No frequency scaling, no deep sleep states. This will be revisited in the future, when no job runs, it doesn't make sense to run at full frequency.

## Timers and clocks

Userspace threads have access to high-precision timing via the TSC (Time Stamp Counter). RDTSC/RDTSCP is a single instruction (~20ns) providing nanosecond-resolution timestamps without syscalls or MMIO. The kernel parses ACPI HPET and FADT timer metadata (HPET, PM timer) for platform timing support while keeping TSC as the primary runtime clock source.

Cross-node time synchronization (mechanism TBD; PTP, GPS, or similar) will provide consistent timestamps across all machines in the cluster. Threads on different nodes will be able to coordinate using a shared time base.

## Designing for this architecture

### Partition your data

Each thread should own a partition of the problem. For graph processing, partition vertices. For databases, partition keys. For simulations, partition the grid. Minimize cross-partition reads.

### Align shared structures to page boundaries

The DSM operates at 4KB page granularity. Align each thread's shared slice to page boundaries so that a single page is not split across two writers' slices.

Within a single machine, false sharing occurs at the 64-byte cache line level. Pad shared structures to cache-line boundaries.

### Accept stale reads

The shared region is read-heavy by design. Readers may see slightly stale data. This is fine for most workloads:
- Connection tracking: a new connection might not be visible for a few microseconds.
- Graph processing: vertex values from the previous iteration are acceptable (this is how BSP-model graph processing works anyway).
- Aggregation: partial results converge over iterations.

If you need strong consistency for a specific operation, use sequence counters on the writer side.

### Co-locate readers with writers

Each shared page has exactly one writer, permanently. Readers that access a writer's pages frequently should be co-located (NUMA affinity) with that writer to minimize remote fetches.

## Fault tolerance

### Thread failure

If your thread crashes or stops heartbeating, BSP kills it, resets its NIC queue, and re-instantiates it on the same core. Private memory is lost. The task starts over from scratch. Shared page ownership is restored from backups (shared pages are duplicated).

Your thread's shared write slot becomes untrusted. Other threads stop reading from it until BSP scrubs it.

### Node failure

If an entire node dies, all threads on that node must be rescheduled. BSP places them on available cores elsewhere in the cluster, respecting NUMA affinity rules (threads that were co-located stay co-located if possible). If no cores are available, tasks are queued until cores are freed. If your threads are inter-dependent, leave a node as hot-spare.

All shared pages are replicated to a backup node. On node failure, the backup is promoted to primary; no shared data is lost.

### Performance after failover

A rescheduled thread keeps its original shared memory mappings at the same virtual addresses. Your code does not need to change; pointers still work. However, shared pages that were tier 1 or tier 2 at the original placement may now be tier 2 or tier 3 from the new location. Your thread silently pays higher latency for those accesses.

The locality map is not updated after failover. It still reflects the original placement. This is by design: failover is a catastrophic event, not normal operation. Correctness is preserved; performance degrades.

### What you should do

- If this fits your use case, use machine anti-affinity for redundancy: if the same task runs on two machines, one survives node failure.
- Design tasks to be restartable. A thread should be able to start over without corrupting the rest of the system.
- Accept that post-failover performance will be degraded. If this matters, re-deploy the application with fresh placement.

## Comparison with Linux for HPC

> **Note:** This comparison describes the target architecture. The kernel currently runs on a single node. The multi-node features described below (DSM, cross-machine memory, machine anti-affinity for fault tolerance) are not yet implemented.

This section compares Isurus with Linux-based HPC stacks across dimensions that matter for supercomputing workloads. The goal is to help you decide which platform fits your use case, not to argue that one is universally better.

### Why Isurus instead of Linux

On Linux, getting bare-metal performance from an HPC workload requires fighting the kernel at every step: isolcpus, DPDK, SPDK, taskset, hugepages, NUMA pinning, RT scheduling, cgroups. Each is a workaround against a general-purpose OS that was not designed for your workload. These workarounds interact badly, break across kernel upgrades, and require expert-level tuning to get right.

Isurus eliminates the entire category of "kernel bypass." There is nothing to bypass. The design assumptions match HPC workloads from the start:

- **Minimal jitter.** No OS-generated interrupts on application cores, no scheduler, no frequency scaling, no context switches. CPUs run at maximum capacity at all times. The only remaining jitter sources are hardware-level (SMIs, NMIs, MCEs), which are infrequent. For latency-sensitive workloads, this means predictable tail latency. For throughput-oriented workloads, this means no cycles lost to OS overhead.
- **Shared memory across machines without MPI.** A pointer dereference works across the cluster. No serialization, no message passing, no explicit data movement. For workloads with irregular, fine-grained access patterns (graph processing, sparse computation, distributed databases), this eliminates the entire messaging layer.
- **NUMA-correct by default.** On Linux, getting NUMA right is expert-level work and fragile. On Isurus, every allocation is NUMA-local, every thread is pinned, the locality map tells you the cost of every access. You cannot accidentally get it wrong.
- **Single-purpose deployment.** One process, one cluster, one workload. No containers, no orchestration, no package management, no multi-tenant isolation. You deploy a workload and every core runs it.

The trade-off is ecosystem. Isurus has no third-party libraries, no standard debuggers, no safety net. It is for teams who know exactly what they want to run and are willing to write against a bare-metal API to get maximum performance. If your team spends significant engineering effort tuning Linux to get out of the way, Isurus is a system where that tuning is the default.

The sections below compare each dimension in detail.

### I/O path

**Trade-off.** Linux has decades of driver support for virtually every device on the market. Isurus supports only the devices that have been explicitly implemented. If your hardware is not supported, Linux is your only option.

### Scheduling and thread model

**Linux.** General-purpose preemptive scheduler with thousands of threads, migration, load balancing, and CFS. You can pin threads with taskset or cgroups, but it is opt-in and fragile; a misconfigured cgroup or a stray system thread can still steal cycles.

**Isurus.** One thread per core, pinned at creation, no context switching, no scheduler overhead. The core belongs entirely to your thread. BSP handles all interrupts so application threads are never preempted.

**Trade-off.** Linux handles mixed workloads well (batch jobs, interactive services, system daemons coexisting). Isurus is single-purpose; every core except BSP runs application code. If you need to run unrelated services alongside your HPC workload, Linux is the better choice.

### Cross-machine memory

**Linux.** No shared memory across machines. Applications must use MPI, RDMA verbs, or application-level protocols. Data that crosses machine boundaries must be explicitly serialized and transferred.

**Isurus.** DSM provides a shared address space across machines. A pointer dereference to remote data just works; the page fault and network fetch happen transparently behind the scenes.

**Trade-off.** DSM adds page-fault latency on first remote access. MPI can batch transfers more efficiently for bulk data movement where access patterns are known in advance. DSM is better for fine-grained, irregular access patterns where the application cannot predict which data it will need next.

### NUMA awareness

**Linux.** NUMA is a secondary heuristic. Tools like numactl, mbind, and set_mempolicy exist but are opt-in. The default allocator may place memory on the wrong node, and the scheduler may migrate threads away from their data.

**Isurus.** NUMA is the primary abstraction. All allocations are NUMA-local by default. Affinity constraints are specified at thread creation. The locality map tells your application the cost tier of every shared region.

**Trade-off.** Linux is more flexible; you can change NUMA policy at runtime, migrate threads, and rebalance dynamically. Isurus's static placement is simpler and more predictable but less adaptive to changing workload patterns.

### Ecosystem and tooling

**Linux.** Mature ecosystem. GDB, perf, valgrind, strace, thousands of libraries, package managers, container runtimes, decades of documentation and community knowledge.

**Isurus.** Research prototype. Minimal tooling. No standard library beyond what the kernel provides. Application code must be written directly against the kernel's APIs.

**Trade-off.** This is the biggest practical barrier. Linux wins overwhelmingly on ecosystem maturity. If your application depends on third-party libraries, debugging tools, or container infrastructure, the cost of porting to Isurus may outweigh the performance benefits.

### Fault tolerance

**Linux.** Process isolation via virtual memory, mature error handling, and decades of hardening. A crashed process does not take down the system. Other processes continue unaffected.

**Isurus.** Thread isolation via per-thread page tables. Shared pages are replicated to backup nodes. Heartbeat-based failure detection triggers automatic thread re-instantiation. The single-writer principle limits corruption scope.

**Trade-off.** Linux isolates processes from each other completely. Isurus runs a single process spanning the cluster; a thread failure can affect shared state. The single-writer principle and BSP scrubbing mitigate this, but the blast radius of a failure is larger than Linux process isolation.

### When Isurus is a better fit

**Single node (bare-metal low-jitter OS):**

- **Latency-sensitive I/O:** packet processing, storage, high-frequency trading. Minimal jitter, direct device access, no tuning required. What Linux HFT shops build with isolcpus/DPDK/hugepages/RT scheduling is the default here.
- **Streaming and continuous processing:** sensor ingestion, telemetry, market data, log processing. One thread per core, polling, no interrupt overhead.
- **Fully independent workloads:** independent work units with minimal cross-thread communication. Brute-force search, per-request processing. Each core runs undisturbed.

**Cluster (shared memory across machines):**

- **Fine-grained shared state with irregular access patterns:** large-scale graph processing (graphs too large for GPU memory, random traversals), stateful firewalls (shared connection tables). DSM eliminates the messaging layer for workloads where you cannot predict which partition you will access next.
- **Shared aggregation and reduction:** per-thread partial results read by a coordinator via shared memory. No gather phase, no messages.
- **NUMA-sensitive workloads** that suffer from Linux's scheduler and allocator defaults.
- **Single-purpose clusters** dedicated to one workload, not shared multi-tenant environments.

### When Linux is a better fit

- Mixed workloads on shared infrastructure.
- Applications that need a broad ecosystem (libraries, debugging tools, container support).
- Workloads where MPI's bulk transfer model is more efficient than fine-grained DSM.
- Teams without the expertise to develop against a bare-metal kernel API.
