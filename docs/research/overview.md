# Isurus - Research Overview

> **Note:** This document describes the target architecture. The kernel currently runs on a single node. Multi-node clustering, DSM, DPDK, and SPDK are not yet implemented. Sections below describe the intended design, not the current state.

## 1. Project Overview

Isurus is an **operating system for AMD64 (x86-64)** where the kernel runs on its own dedicated core (BSP) and never sits in the application's data path. Application threads own their cores entirely: they access devices directly via mapped MMIO, poll NICs and storage without syscalls, and are never interrupted by the kernel. BSP handles coordination separately -- setup, memory management, coherence, fault recovery -- but once threads are running, the kernel is not between them and the hardware.

On a **single machine**, this gives you a bare-metal execution environment with minimal jitter, direct device access (DPDK-style NIC polling, SPDK-planned storage), and CPUs locked at maximum frequency. What Linux HPC and HFT shops achieve through extensive tuning (isolcpus, DPDK, hugepages, RT scheduling, NUMA pinning) is the default configuration here.

On a **cluster**, Isurus extends this model across machines. Multiple nodes are treated as **one large NUMA topology**. A single process spans all machines, threads share a **single logical address space** via distributed shared memory, and remote memory access is transparent (a page fault fetches the data over layer 2).

The kernel is implemented as a **research prototype** to evaluate:

* an OS designed for maximum performance with minimal abstraction overhead
* NUMA-aware scheduling as a primary system primitive
* locality-aware memory management
* remote NUMA domains spanning multiple machines
* execution of threads belonging to a single process across distributed CPUs

---

## 2. Design Doctrine: Eliminate, Don't Detect

Kernel bugs compound silently. A userspace crash tells you where it
happened; a kernel-space bit flip just waits, and when the symptom
eventually appears it is typically far from the cause. Runtime
checks (assertions, sanitizers, watchdogs) help only when the check
fires on the same build that caused the damage; they do not help
when the bug was introduced three releases ago and only manifests
when the kernel grows past a threshold and the damage starts landing
on live code.

The design principle of Isurus is therefore: **remove categories of
bug by construction rather than trying to detect them at runtime.**
Wherever a traditional kernel says "use a lock," "retry on
collision," "handle the race," or "log and recover," Isurus prefers
to shape the system so the hazard cannot occur in the first place.

This is bought by accepting several upfront tradeoffs that do not
bend later:

- **Fixed sizing.** Every AP thread declares its working set as a
  static struct at build time. `sys_kmalloc` is forbidden from AP
  context. No runtime allocator on the data path, no fragmentation,
  no out-of-memory branch to forget.
- **No context switching on APs.** One thread per core, pinned at
  creation, never migrated. No interrupts on the data path (BSP
  handles all IRQs).
- **Single-writer ownership.** Each shared memory slice belongs
  permanently to one thread. Other threads may read. Ownership never
  transfers. Concurrent writes are not "resolved" - they cannot
  happen.
- **Per-thread address space.** Each AP loads its own CR3. A buggy
  thread cannot scribble on another thread's memory; hardware
  enforces isolation.
- **Polled I/O only.** NICs and (planned) storage controllers are
  mapped into ring 3; threads poll their hardware directly. No
  interrupt-driven I/O, no IRQ-timing Heisenbugs, no jitter from
  kernel callbacks.
- **Placement as the only scheduling decision.** The scheduler runs
  once per thread creation and on node failure. There is no
  preemption, no continuous scheduler, no migration policy to
  tune or get wrong.

What these choices exclude:

| Excluded bug class | By-construction reason |
|---|---|
| Data-path allocator corruption | No runtime allocator on APs |
| Race conditions on AP data | One thread per core, no context switch |
| Lock contention / priority inversion | No locks on the AP data path |
| Cache-coherence races on shared data | Single-writer; readers never contend |
| Cross-thread memory corruption | Per-thread CR3 isolation |
| Interrupt-timing Heisenbugs | Polled I/O, no IRQs on AP data path |
| Scheduler-induced non-determinism | Placement is static once decided |
| TLB shootdown storms | Per-thread page tables, no cross-CPU invals on normal ops |

The price is honest: the system must be *sized* in advance. An
application declares how much memory its per-core thread will use;
the kernel declares how many tasks, how many NICs, how many AP
stacks at build time. Users who need "grow on demand" with unknown
working sets are not the target.

**Scope of the doctrine.** The correctness-by-construction guarantee
applies from long mode onward - i.e., once the kernel has finished
its 16 -> 32 -> 64 bit boot transition and is running 64-bit code
with paging enabled. The early boot transition (`src/boot/kmain.s`,
the `asm16_*` and `asm32_*` include files) is legacy x86
ceremony required by the hardware; it pre-dates the doctrine and
does not inherit its guarantees. Touch that code with suspicion and
measure (e.g., by snapshotting memory after boot and diffing against
the binary image) whenever you change it; the 32-bit boot stack bug
fixed on 2026-04-22 is an example of a latent hazard that lived
there for years.

**For contributors.** Any new subsystem, allocator, scheduler
primitive, or driver should be checked against this principle
before it lands. If your addition needs a lock on a hot path, a
runtime heap allocation on the AP data path, a preemption point,
or a handler that can race with another core's handler, stop.
Either find a shape that removes the hazard, or write down clearly
why this is one of the bounded places where the doctrine yields
(e.g. BSP cooperative multitasking - yielding is explicit and the
critical sections are bounded).

---

## 3. Motivation

Modern multi-socket machines rely on **NUMA (Non-Uniform Memory Access)** architectures.

In these systems:

* each CPU socket has local memory
* accessing local memory is significantly faster than accessing memory attached to another socket

Traditional operating systems assume a **global shared memory abstraction**, using NUMA optimizations as secondary heuristics.

This often leads to:

* excessive remote memory traffic
* cache-line bouncing
* poor scaling at high core counts

The premise of this project is that **memory locality should be a fundamental scheduling constraint**, not merely an optimization.

---

## 4. Limitations of Current Distributed Computing

When workloads exceed the capacity of a single machine, scaling typically relies on **cluster computing**.

In cluster environments:

* processes run on separate nodes
* communication occurs through a full network protocol stack
* applications exchange serialized messages

Typical communication path:

Application - OS networking stack - TCP/IP - network driver - Ethernet - remote OS - application

Even within a single rack this introduces:

* protocol overhead
* multiple memory copies
* kernel transitions
* serialization and deserialization

Applications must therefore be explicitly designed as **distributed systems**.

This prevents transparent scaling of single-process workloads.

---

## 5. Existing Hardware Model: Remote Memory Access

High-performance computing environments provide mechanisms such as **Remote Direct Memory Access (RDMA)** over **InfiniBand**.

RDMA allows:

* direct read/write access to memory on a remote node
* minimal CPU involvement during transfers

However RDMA provides **remote memory access only**.

It does not allow:

* remote CPUs to execute threads belonging to the same process
* unified process scheduling across nodes
* a shared address space across machines

The programming model therefore remains distributed.

---

## 6. Core Concept

The central idea of this research is to treat a cluster of machines as a **distributed NUMA system**.

Nodes communicate over a **dedicated internal network** at the Ethernet layer (OSI layers 1-2), bypassing the full TCP/IP stack. In a real world application, Infiniband, HPE Cray Slingshot or similar would be a proper vessel.

Within this environment:

* a **single logical process may span multiple machines**
* threads may execute on processors located on different nodes
* devices (NICs, storage controllers) are accessed directly from userspace via polling (DPDK; SPDK planned), bypassing interrupt-driven I/O entirely
* all threads share a **single logical shared address space**

Remote machines are modeled as **NUMA regions with higher latency**.

This allows the system to behave conceptually as **one extremely large NUMA computer**.

---

## 7. Thread Placement Model

Each thread is **pinned to a core** at creation. One thread per core. No context switching, no migration during normal operation.

A thread's core determines its NUMA node, which determines its local memory and its NIC. The thread polls its NIC directly (DPDK-style) and never receives interrupts; BSP handles all interrupts.

The only ring 0 entry on an AP core is a DSM page fault (remote shared memory access).

### Affinity constraints

Applications specify placement constraints at thread creation. Three levels of locality:

* **NUMA affinity:** threads that share data heavily should be on cores within the same NUMA node. They read each other's shared memory slices via local NUMA reads.
* **Machine affinity:** threads that can tolerate cross-NUMA latency but must not pay for network round trips. Hardware cache coherence still applies within a single machine.
* **No constraint:** threads that interact rarely. Can be placed on different machines. Cross-machine reads go through DSM (page faults over the inter-node NIC).

Anti-affinity constraints also exist:

* **NUMA anti-affinity:** spread threads across NUMA nodes for NIC bandwidth (each node has its own NIC).
* **Machine anti-affinity:** spread threads across machines for fault tolerance (node failure does not kill all related threads).

---

## 8. Scheduler

The scheduler is a **one-time placement solver**, not a continuous scheduler.

At thread creation, it takes the set of threads, their affinity and anti-affinity constraints, and the cluster topology, and produces a core assignment for each thread. Once placed, threads do not move.

The scheduler runs again only on **node failure**: when BSP detects a dead node (heartbeat timeout), the dead threads' tasks are reassigned to available cores elsewhere in the cluster, respecting affinity constraints. If no core is available, the task is queued until a core is freed.

There is no load balancing, no migration, no preemption. Placement is the only scheduling decision

---

## 9. Memory Management Model

Memory allocations are associated with specific **NUMA regions**.

Allocation policies include:

* local-node allocation
* controlled remote allocation
* explicit NUMA region mapping

This enables memory structures to remain physically close to the threads that operate on them.

The design aims to reduce:

* remote memory access latency
* cross-node cache invalidation
* global TLB shootdowns, which implies a distributed IPI propagation.

---

## 10. Remote NUMA Model

The system extends NUMA topology beyond a single machine.

Remote nodes are treated as additional NUMA regions.

Conceptually:

Local node - low latency memory access
Remote node - higher latency memory access

From the kernel perspective, the difference between:

* another socket
* another machine

becomes a **matter of latency distance within the NUMA topology**.

---

## 11. Distributed Shared Memory Architecture

The architecture effectively implements a **distributed shared memory (DSM) system**.

Characteristics include:

* a unified shared address space
* distributed execution across nodes
* locality-aware scheduling
* memory ownership associated with NUMA domains

Applications operate as if running on **one large shared-memory machine**, rather than a distributed cluster.

---

## 12. Resource Aggregation

A process spanning multiple machines can leverage hardware resources across nodes, including:

* processors from multiple systems
* south bridges from multiple machines
* I/O devices across machines, each accessed directly from userspace via polling (DPDK for NICs; SPDK for storage, planned)

This allows the system to aggregate compute and I/O resources for a **single process execution context**.

---

## 13. Memory Consistency and Coherence

Defining a coherent memory model across nodes was a critical research component. The architecture resolves it as follows:

* **Cache coherence between machines:** handled by a DSM layer built into the kernel. Remote pages are fetched on demand via page faults over the layer 2 inter-node NIC. Within a single machine, hardware cache coherence applies normally.
* **Permanent ownership:** single-writer / multiple-reader. Each shared slice belongs permanently to the thread that created it. Ownership never transfers. Other threads have read-only access.
* **Cache invalidation:** when a writer modifies a page, the coherence protocol invalidates all cached read-only copies on remote nodes. Readers fetch a fresh copy on next access.
* **Write contention:** eliminated by design. The single-writer principle ensures each shared memory location has exactly one writer. There is no concurrent write access to resolve.

BSP tracks which nodes hold cached copies of which pages. Invalidation is coordinated through BSP.

---

## 14. Failure and Fault Semantics

Unlike traditional SMP systems, nodes in a remote NUMA cluster may fail independently. The architecture addresses this:

* **Loss of memory regions:** all shared pages are replicated to a backup node on a different physical machine. On node failure, BSP promotes the backup to primary and broadcasts the new ownership map. No shared data is lost.
* **Partial process degradation:** a dead thread's task is re-instantiated on the same core if possible, or queued for rescheduling on available cores elsewhere. The rest of the process continues running. Affinity constraints are respected during rescheduling.
* **Remote page faults to unreachable nodes:** if a node is unreachable, any thread faulting on a page owned by that node stalls until BSP completes failover (backup promotion). Once the backup is promoted, the fault resolves against the new primary.
* **Shared state consistency:** a dead thread's shared write slot is untrusted until BSP scrubs it. Other threads do not read from a slot whose owner is dead (single-writer principle: the writer is gone, so the data may be partial).

---

## 15. Research Evaluation Goals

The prototype kernel enables experimental evaluation of:

* scheduling overhead across nodes
* remote memory access latency
* scalability relative to traditional NUMA systems
* comparison with cluster-based approaches

Evaluation targets workloads where shared memory across threads and nodes is essential:

* **Distributed connection tracking / stateful firewall.** Each thread processes packets from its NIC queue, but a TCP connection may arrive on any thread (different source ports, different RSS hashes). The connection table must be shared. Read-heavy, write-on-new-connection. Without shared memory, every thread needs to query a central service for every packet. With shared memory, threads read the table directly; local NUMA read if the entry is on their node, remote NUMA read otherwise.

* **Graph processing (PageRank, BFS, community detection).** Graph partitioned across nodes by vertex. Each thread processes local vertices. But edges cross partitions; a vertex on node A has neighbors on node B. In each iteration, threads need to read neighbor values. With DSM, a thread reads the remote vertex value directly through the shared address space. Without it, you need explicit scatter-gather rounds (which is what Pregel/GraphX do, and it is what this project is trying to avoid).

* **In-memory distributed database / key-value store.** The cluster IS the storage. Each node owns a partition. Threads serve queries. A query touching keys on another node reads them through shared memory; no serialization, no RPC, just a pointer dereference that happens to fault into the DSM layer. This is the "one large NUMA computer" vision directly. Note: for well-partitioned databases with predictable access patterns, RDMA one-sided reads or optimized RPCs may be faster than DSM page faults (which fetch 4KB per fault). DSM's advantage is transparency and irregular access patterns where the application cannot predict which partition it will need.

* **Shared aggregation / reduction.** All threads produce partial results. A shared buffer holds per-thread slots. Each thread writes its slot (single-writer). A coordinator reads all slots to produce the final result. No gather phase, no messages; just memory reads across NUMA domains.

* **Exchange query + forward (HFT-style).** Each thread fetches latest price via API and sends a packet to a remote endpoint. Latency-critical per request/response path; no shared coordination, per-thread execution only.

The pattern across all these: threads are mostly independent, but they need read access to state owned by other threads. The shared region is read-heavy, write-rare-per-location, and each location has a single writer. This reinforces the single-writer principle: each thread owns its slice of the shared space, writes only to its slice, and reads from others. Isolation protects private state (stack, local heap, working set). The shared region is explicitly opted into.

These experiments aim to determine whether the remote NUMA model provides advantages over traditional clustering.

---

## 16. Research Contributions

The project explores several research questions:

* Can distributed machines be modeled as NUMA regions?
* Can a scheduler effectively manage execution across such regions?
* Can a shared memory abstraction be preserved across nodes without excessive overhead?
* Can a single process scale beyond the hardware limits of a single machine?

The kernel prototype provides a platform for investigating these questions.

---

## 17. Conclusion

This work proposes a kernel architecture where **NUMA locality becomes a primary operating system abstraction**.

The kernel provides each thread with a static locality map describing the cost tier of every shared memory region.
The scheduler uses affinity and anti-affinity constraints to determine thread-to-core placement at creation.
Memory allocation respects NUMA topology.

By extending NUMA concepts across machines, the system explores a model in which **multiple physical computers cooperate as a unified shared-memory system**, enabling a single process to utilize distributed processors and memory resources while maintaining a coherent execution environment.
