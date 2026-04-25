<p align="center">
  <img src="isurus_logo.png" alt="Isurus logo" width="100%">
</p>

# Isurus - Zero-Interrupt NUMA Cluster

A bare-metal OS kernel for x86-64. The idea: treat a cluster of machines as one big NUMA computer, where a single process spans multiple physical nodes and threads share memory transparently across machines.

## Why "Isurus"

*Isurus* is the genus of the mako shark. Mako sharks are obligate ram ventilators: they must keep swimming to breathe. They never stop, they never sleep, they never idle. They have warm-blooded muscles in an otherwise cold-blooded lineage -- specialized engineering for sustained high performance. They are the fastest sharks in the ocean, solitary pelagic hunters.

The metaphor maps directly to the kernel's design: threads never yield, CPUs never throttle, cores run at full speed continuously, each thread owns its hardware and hunts alone. Stop moving and you die.

## The problem

Scaling a workload beyond one machine today means rewriting it as a distributed system:

- **Linux + MPI:** explicit message passing. You serialize, send, deserialize. Your algorithm disappears under communication boilerplate.
- **Kubernetes / microservices:** network boundaries everywhere. Every cross-node interaction is an RPC with serialization overhead and failure modes.
- **RDMA (InfiniBand):** gives you remote memory reads, but not remote execution. You still manage two separate address spaces and coordinate manually.
- **Historical DSM (Treadmarks, Ivy):** bolted on top of a general-purpose OS. Context switches, syscall overhead, full networking stack in the data path. They proved the concept but couldn't deliver the performance.

The common thread: the OS gets in the way.

## What Isurus does differently

- **Cluster as NUMA topology.** Remote machines are just NUMA nodes with higher latency. Same abstraction, different numbers.
- **One thread per core, pinned.** No scheduling, no migration, no context switching. Placement is decided once at creation.
- **Distributed shared memory.** Threads share a single address space across machines. Access a remote page and you get a page fault that fetches it over layer 2. Transparent to the application.
- **Single-writer coherence.** Each shared memory slice has exactly one writer, permanently. Ownership never transfers. No write contention by design.
- **Direct device access.** NICs and storage controllers are mapped into userspace. Threads poll devices directly (DPDK-style for NICs, SPDK planned for storage). No syscalls for I/O.
- **No dynamic power management.** P-states locked to max frequency, no deep C-states (C1/HLT is the floor), T-states ignored. Cool your hardware appropriately.

This is not a general-purpose OS. Not Linux. Not for running Docker containers, web servers, or desktop applications. There is no POSIX and no dynamic linking.

## Hardware model

```mermaid
flowchart TB
    subgraph N0["NUMA node 0"]
        direction LR
        DRAM0["DRAM<br/>(local)"]
        subgraph S0["Socket 0"]
            direction TB
            BSP["CPU 0 -- BSP<br/>kernel + shell + IRQs"]
            AP1["CPU 1 -- AP<br/>pinned ring-3 thread"]
            AP2["CPU 2 -- AP<br/>pinned ring-3 thread"]
        end
        PCIe0["PCIe root 0"]
        NIC0["NIC 0"]

        DRAM0 --- S0
        BSP --- PCIe0
        AP1 --- PCIe0
        AP2 --- PCIe0
        PCIe0 --- NIC0
    end
    subgraph N1["NUMA node 1"]
        direction LR
        DRAM1["DRAM<br/>(local)"]
        subgraph S1["Socket 1"]
            direction TB
            AP3["CPU 3 -- AP<br/>pinned ring-3 thread"]
            AP4["CPU 4 -- AP<br/>pinned ring-3 thread"]
            AP5["CPU 5 -- AP<br/>pinned ring-3 thread"]
        end
        PCIe1["PCIe root 1"]
        NIC1["NIC 1"]

        DRAM1 --- S1
        AP3 --- PCIe1
        AP4 --- PCIe1
        AP5 --- PCIe1
        PCIe1 --- NIC1
    end
    N0 -. "cross-NUMA -- higher latency" .- N1
```

Each socket sits between its local DRAM and its local PCIe root.
The PCIe root is the gateway between the cores and external devices;
the NIC sits one hop past the root, on the device side. Every core
in a socket reads and writes its local DRAM and drives the local
NIC through the local PCIe root. The dashed link between sockets is
the only path to cross-NUMA memory or to a remote socket's NIC, and
it pays the higher latency.

**CPU 0 (the BSP) is the only core that runs the kernel, the shell,
and handles interrupts -- it is also the only core that ever sees a
syscall.** Every other core is an AP: exactly one pinned ring-3
thread for its lifetime, no scheduling, no migration, no context
switching, no kernel-mode work in the data path. Placement is the
only scheduling decision.

(Two sockets and three cores per socket above is illustrative; the
model scales to N sockets and N cores per socket.) See
[docs/research/overview.md section 7](docs/research/overview.md)
for the execution model and
[docs/developer/architecture.md](docs/developer/architecture.md)
for the implementation.

## Design doctrine

The kernel is meant to be **as small and basic as possible**, retaining maximum performance and minimum latency, yet with a very optimized design for the targeted tasks. Everything in the system is shaped around one principle: **eliminate categories of bug by construction rather than detect them at runtime.**

Kernel bugs compound silently -- a bit-flip deep in the kernel may sit dormant for years until a layout shift brings it to a live code path. Runtime checks (asserts, sanitizers, watchdogs) only help on the build that caused the damage, not on the build where it finally fires. So Isurus accepts several up-front tradeoffs -- static sizing, one-thread-per-core, no context switching on APs, single-writer shared memory, per-thread address spaces, polled I/O only, placement as the only scheduling decision -- and in exchange excludes whole classes of bug (data-path allocator corruption, AP data races, lock contention, cache-coherence races, interrupt-timing Heisenbugs, scheduler non-determinism). You pay by sizing your working set ahead of time; you gain a system that is correct by construction within its envelope.

See [Research Overview, section 2](docs/research/overview.md) for the full doctrine and what each design choice removes.

**Single node:** a hard real-time, ultra-low-latency execution environment. One thread per core, no interrupts on application cores, CPUs at max frequency, direct device polling. Use cases: high-frequency trading, packet processing, real-time sensor ingestion, any workload where deterministic per-core execution matters.

**Cluster:** heavy parallelism across dedicated hardware. A single process spans multiple machines, threads share memory transparently via DSM, each thread owns its core and its NIC. Use cases: large-scale graph processing, distributed in-memory databases, shared aggregation across hundreds of cores.

## What works today

> The kernel currently runs on a single node. Multi-node clustering, DSM, DPDK, and SPDK are not yet implemented.

- ISO-first BIOS boot flow (El Torito no-emulation boot via xorriso)
- Boots on x86-64 (real hardware and QEMU Q35)
- SMP: AP bringup via INIT-SIPI-SIPI
- ACPI: MADT, SRAT, SLIT, HPET, FADT, MCFG, DMAR/IVRS parsing with query APIs
- PCI device enumeration (PCIe ECAM via MCFG)
- Virtio-net driver and NIC abstraction layer
- Ring 3 shell on BSP with SYSCALL/SYSRET and cooperative multitasking
- FAT32 read-only filesystem, IDE driver
- kmalloc/kfree with bitmap allocator (4KB granularity)
- Per-CPU structures, per-CPU TSS
- 80x28 text mode VGA driver

## Quick start

```bash
scripts/compile_qemu.sh
```

## Documentation

- **[Getting Started](docs/getting-started/building.md)** -- build toolchain, compile scripts, QEMU setup
- **[Research Overview](docs/research/overview.md)** -- motivation, core concepts, evaluation goals
- **[Application Model](docs/user/application-model.md)** -- execution model, memory model, how to write applications for Isurus
- **[Architecture](docs/developer/architecture.md)** -- kernel design decisions and rationale
- **[Full Documentation Index](docs/README.md)** -- all documentation organized by audience

## License

Dual licensed: AGPLv3 for non-commercial use, commercial license available. See [LICENSE](LICENSE).

## Contact

Pierre Ancelot -- pierreact at gmail dot com
