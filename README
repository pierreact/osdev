To compile and run the kernel, run compile_qemu.sh
It certainly will complain at first you don't have the right tools to run it... just install them :)


# Remote NUMA Kernel — Research Prototype for Distributed NUMA Computing

## 1. Project Overview

This project is an experimental **operating system kernel for AMD64 (x86-64)** designed to explore a scheduling and memory model based on **NUMA locality** and an extension called **remote NUMA**.

The objective is to determine whether **a single process can transparently execute across multiple machines while sharing a single logical memory space**.

The kernel is implemented as a **research prototype** to evaluate:

* NUMA-aware scheduling as a primary system primitive
* locality-aware memory management
* remote NUMA domains spanning multiple machines
* execution of threads belonging to a single process across distributed CPUs

Rather than treating distributed systems as independent nodes communicating through networking protocols, the system models a cluster as **one large NUMA topology** on two layers, local vs remote machine.

---

# 2. Motivation

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

# 3. Limitations of Current Distributed Computing

When workloads exceed the capacity of a single machine, scaling typically relies on **cluster computing**.

In cluster environments:

* processes run on separate nodes
* communication occurs through a full network protocol stack
* applications exchange serialized messages

Typical communication path:

Application → OS networking stack → TCP/IP → network driver → Ethernet → remote OS → application

Even within a single rack this introduces:

* protocol overhead
* multiple memory copies
* kernel transitions
* serialization and deserialization

Applications must therefore be explicitly designed as **distributed systems**.

This prevents transparent scaling of single-process workloads.

---

# 4. Existing Hardware Model: Remote Memory Access

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

# 5. Core Concept

The central idea of this research is to treat a cluster of machines as a **distributed NUMA system**.

Nodes communicate over a **dedicated internal network** at the Ethernet layer (OSI layers 1–2), bypassing the full TCP/IP stack. In a real world application, Infiniband, HPE Cray Slingshot or similar would be a proper vessel.

Within this environment:

* a **single logical process may span multiple machines**
* threads may execute on processors located on different nodes
* threads benefit of horizontal scaling capabilities for interrupts processing.
* all threads share a **single logical virtual address space**

Remote machines are modeled as **NUMA regions with higher latency**.

This allows the system to behave conceptually as **one extremely large NUMA computer**.

---

# 6. Thread Locality Model

Each thread maintains locality metadata describing its preferred execution environment.

Attributes include:

* preferred CPU
* NUMA node affinity
* memory region dependencies
* thread affinity and anti-affinity rules

This metadata allows the kernel scheduler to reason about **execution placement relative to memory locality**.

Threads retain their locality information even when migrated across nodes.

---

# 7. Scheduler Architecture

The scheduler is designed around explicit **NUMA topology awareness**.

Scheduling decisions prioritize:

1. CPUs within the thread's local NUMA node
2. CPUs close to the thread's memory regions
3. remote nodes only when local compute resources are insufficient

Remote execution therefore represents a **higher-latency scheduling domain**, not a separate distributed system.

Migration policies attempt to balance:

* load distribution
* memory locality
* inter-node traffic

---

# 8. Memory Management Model

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

# 9. Remote NUMA Model

The system extends NUMA topology beyond a single machine.

Remote nodes are treated as additional NUMA regions.

Conceptually:

Local node → low latency memory access
Remote node → higher latency memory access

From the kernel perspective, the difference between:

* another socket
* another machine

becomes a **matter of latency distance within the NUMA topology**.

---

# 10. Distributed Shared Memory Architecture

The architecture effectively implements a **distributed shared memory (DSM) system**.

Characteristics include:

* a unified virtual address space
* distributed execution across nodes
* locality-aware scheduling
* memory ownership associated with NUMA domains

Applications operate as if running on **one large shared-memory machine**, rather than a distributed cluster.

---

# 11. Resource Aggregation

A process spanning multiple machines can leverage hardware resources across nodes, including:

* processors from multiple systems
* south bridges from multiple machines
* interrupt processing capacity from different machines

This allows the system to aggregate compute and I/O resources for a **single process execution context**.

---

# 12. Memory Consistency and Coherence Challenges

A critical research component is defining a **coherent memory model across nodes**.

Key problems include:

* cache coherence between machines
* remote page ownership
* distributed TLB invalidation
* write contention on shared memory

The system explores policies for:

* memory region ownership
* remote invalidation mechanisms
* page migration between nodes

These mechanisms determine the scalability and practicality of the remote NUMA model.

---

# 13. Failure and Fault Semantics

Unlike traditional SMP systems, nodes in a remote NUMA cluster may fail independently.

The architecture therefore considers:

* loss of memory regions due to node failure
* partial process degradation
* remote page faults caused by unreachable nodes

Failure semantics must define how the system behaves when **portions of the distributed memory space disappear**.

---

# 14. Research Evaluation Goals

The prototype kernel enables experimental evaluation of:

* scheduling overhead across nodes
* remote memory access latency
* scalability relative to traditional NUMA systems
* comparison with cluster-based approaches

Evaluation may involve workloads such as:

* large shared-memory simulations
* graph processing
* in-memory analytics
* HPC workloads with shared state

These experiments aim to determine whether the remote NUMA model provides advantages over traditional clustering.

---

# 15. Research Contributions

The project explores several research questions:

* Can distributed machines be modeled as NUMA regions?
* Can a scheduler effectively manage execution across such regions?
* Can a shared memory abstraction be preserved across nodes without excessive overhead?
* Can a single process scale beyond the hardware limits of a single machine?

The kernel prototype provides a platform for investigating these questions.

---

# 16. Conclusion

This work proposes a kernel architecture where **NUMA locality becomes a primary operating system abstraction**.

Threads maintain explicit locality information.
The scheduler uses this information to determine execution placement.
Memory allocation respects NUMA topology.

By extending NUMA concepts across machines, the system explores a model in which **multiple physical computers cooperate as a unified shared-memory system**, enabling a single process to utilize distributed processors and memory resources while maintaining a coherent execution environment.

Thank you for reading.

Pierre Ancelot

pierreact at gmail dot com
