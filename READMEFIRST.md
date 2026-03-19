# ZINC - What Is This

A bare-metal OS kernel for x86-64. The idea: treat a cluster of machines as one big NUMA computer, where a single process spans multiple physical nodes and threads share memory transparently across machines.

## The problem

Scaling a workload beyond one machine today means rewriting it as a distributed system:

- **Linux + MPI:** explicit message passing. You serialize, send, deserialize. Your algorithm disappears under communication boilerplate.
- **Kubernetes / microservices:** network boundaries everywhere. Every cross-node interaction is an RPC with serialization overhead and failure modes.
- **RDMA (InfiniBand):** gives you remote memory reads, but not remote execution. You still manage two separate address spaces and coordinate manually.
- **Historical DSM (Treadmarks, Ivy):** bolted on top of a general-purpose OS. Context switches, syscall overhead, full networking stack in the data path. They proved the concept but couldn't deliver the performance.

The common thread: the OS gets in the way. Networking stacks, context switches, serialization, kernel transitions. All of it sits between your thread and the data it needs.

## What ZINC does differently

- **Cluster as NUMA topology.** Remote machines are just NUMA nodes with higher latency. Same abstraction, different numbers.
- **One thread per core, pinned.** No scheduling, no migration, no context switching. Placement is decided once at creation.
- **Distributed shared memory.** Threads share a single address space across machines. Access a remote page and you get a page fault that fetches it over layer 2. Transparent to the application.
- **Single-writer coherence.** Each shared memory slice has exactly one writer, permanently. Ownership never transfers. No write contention by design.
- **Direct device access.** NICs and storage controllers are mapped into userspace. Threads poll devices directly (DPDK-style for NICs, SPDK planned for storage). No syscalls for I/O.
- **No dynamic power management.** P-states locked to max frequency, no deep C-states (C1/HLT is the floor), T-states ignored. Cool your hardware appropriately.

## What works today

The kernel currently runs on a single node. Multi-node clustering, DSM, DPDK, and SPDK are not yet implemented. What is working:

- Boots on x86-64 (real hardware and QEMU)
- SMP: AP bringup via INIT-SIPI-SIPI
- ACPI: MADT parsing, LAPIC and IOAPIC initialization
- Per-CPU structures
- FAT32 read-only filesystem, IDE driver
- Kernel shell with commands some commands
- kmalloc/kfree with bitmap allocator (4KB granularity)
- MMIO mapping for APIC region
- 80x28 text mode VGA driver

## Target workloads

### Single node: bare-metal low-jitter OS

ZINC on a single machine gives you what Linux HFT shops spend months building (isolcpus, DPDK, hugepages, RT scheduling, CPU pinning, frequency locking) as the default. No tuning, no fighting the kernel.

- HFT / ultra-low-latency trading (exchange query, forward, per-thread execution)
- Latency-sensitive packet processing
- Real-time sensor ingestion and telemetry
- Any workload where deterministic per-core execution matters more than ecosystem

### Cluster: shared memory across machines

ZINC across multiple nodes is for workloads where threads are mostly independent but need read access to each other's state, with irregular or unpredictable access patterns that map poorly to message passing.

- Stateful firewalls and connection tracking (shared connection tables, read-heavy, write-on-new-connection)
- Large-scale graph processing (graphs too large for GPU memory, irregular traversals like BFS and random walks where you cannot predict which partition you will touch next)
- Shared aggregation and reduction across threads

This is not a GPU oriented OS. It is about CPU cores, memory locality, and network I/O.

## What this is not

Not a general-purpose OS. Not Linux. Not for running Docker containers, web servers, or desktop applications. There is no POSIX, no dynamic linking, no filesystem beyond a read-only FAT32 for loading binaries.

This is a research kernel exploring whether the remote NUMA model can outperform traditional clustering for the workloads listed above.

## Read more

- `README.md` -- research motivation, core concepts, evaluation goals
- `ARCHITECTURE.md` -- design decisions and rationale (memory, execution, DSM, coherence, fault tolerance)
- `doc/user/application-model.md` -- execution model, memory model, how to design applications for this architecture
- `DEBUG.md` -- debugging with GDB and QEMU

---

Pierre Ancelot

pierreact at gmail dot com
