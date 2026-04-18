# GPU Strategy

This document describes the planned approach for GPU support on Isurus. The strategy follows the same design principle as NIC access (DPDK) and storage access (SPDK): the kernel enumerates the device and maps its MMIO regions, the application drives the hardware directly from ring 3 via polling. No kernel driver in the traditional sense.

## Design principle

On Linux, GPU drivers (amdgpu, nvidia, i915) are large kernel modules that manage command submission, memory allocation, interrupt handling, and display output. Applications talk to the driver through ioctl syscalls, and the driver talks to the hardware.

Isurus inverts this. The kernel's only role is PCI enumeration and MMIO mapping. The application thread owns the GPU directly:

```
Linux:    Application -> ioctl -> Kernel driver -> GPU hardware
Isurus:   Application -> MMIO registers -> GPU hardware (direct)
```

This eliminates syscall overhead on the submission path. A compute dispatch or draw call is a register write, not a kernel round-trip.

## How it works

### Kernel side (minimal)

The kernel already enumerates PCI devices and can map BARs via `map_mmio_range()`. For GPU support, the kernel:

1. Identifies the GPU by PCI class (03:00 = VGA, 03:02 = 3D controller) or vendor/device ID.
2. Maps the GPU's BARs (MMIO registers, VRAM aperture) into the requesting thread's page table.
3. Allocates DMA-accessible pages on the thread's NUMA node for command buffers and data.

That is the entire kernel involvement. No GPU-specific driver code in the kernel.

### Userspace side (GPU library)

A userspace library (analogous to DPDK for NICs) handles all GPU interaction:

**1. Device initialization**

Read GPU identification registers via MMIO. Determine the GPU generation, number of compute units, VRAM size, and supported features. Program clock gating, power management registers, and memory controller configuration.

**2. Command submission**

Modern GPUs use ring buffers (command rings) for work submission. The application:

- Allocates a ring buffer in DMA-accessible memory.
- Writes command packets (PM4 on AMD, pushbuffers on NVIDIA) describing the work.
- Writes the ring buffer write pointer to the GPU's doorbell register (a single MMIO write).
- The GPU's command processor reads and executes the commands asynchronously.

This is a direct MMIO write. No syscall. No interrupt. No kernel involvement.

**3. GPU memory management (GPUVM)**

GPUs have their own virtual memory system with their own page tables. On AMD, this is called GPUVM. On NVIDIA, it is the GPU MMU.

The userspace library programs the GPU's page tables via MMIO, mapping DMA-accessible host memory (allocated by the kernel on the thread's NUMA node) into the GPU's virtual address space. The GPU can then DMA to/from these buffers.

Key property: the DMA buffers, the GPU page tables, and the thread driving the GPU are all on the same NUMA node. Zero cross-node traffic for GPU I/O.

**4. Completion detection (polling)**

When the GPU finishes a command, it writes a fence value to a pre-agreed memory location. The application polls this memory location until the expected value appears. No interrupt. No kernel involvement.

This is the same polling model used by CUDA (with `cudaStreamQuery` busy-wait), Vulkan timeline semaphores, and DPDK NIC completion.

**5. No display output**

Isurus targets ML training, HPC, and simulation workloads. No display output is needed. The GPU processes data and writes results to host memory. The application reads the results. Display controller programming (CRTC, encoder, connector, mode setting) is out of scope.

## GPU vendor assessment

### AMD (recommended first target)

AMD publishes register specifications for their GPUs. The Mesa open-source driver project (radeonsi for OpenGL, RADV for Vulkan) contains complete userspace GPU management code that can be referenced.

Advantages:

- Public register documentation.
- Open-source Mesa drivers provide working reference code.
- PM4 command packet format is well-documented.
- GPUVM page table format is documented in AMD's open-source kernel module.
- Active open-source community.

The Mesa radeonsi/radv drivers already do most GPU management from userspace. They rely on the amdgpu kernel driver for four services:

1. **Buffer object allocation** (GEM/TTM). On Isurus: the thread allocates DMA pages from its own pool.
2. **Command ring submission** (ioctl). On Isurus: the thread writes directly to the doorbell register.
3. **GPUVM mapping** (ioctl). On Isurus: the thread programs GPU page tables via MMIO.
4. **Fence/interrupt handling**. On Isurus: the thread polls fence memory.

Each of these is simpler and faster on Isurus than on Linux because there is no kernel intermediary.

### Intel

Intel publishes complete programmer reference manuals (PRMs) for their GPUs. The i915/xe kernel drivers and Mesa's iris/anv drivers are fully open-source.

Advantages:

- Most thoroughly documented GPU architecture.
- Full register-level PRMs publicly available.
- Integrated GPUs share system memory (no discrete VRAM management).

Disadvantages:

- Intel GPUs are less powerful for HPC/ML compute compared to AMD/NVIDIA.
- Less interesting for HPC/ML compute compared to AMD/NVIDIA.

### NVIDIA

NVIDIA does not publish register specifications. Their proprietary driver communicates with the GPU through undocumented firmware interfaces.

- NVIDIA released an open-source kernel module (2022+) with some register definitions, but it is GPL-licensed and the interface is still firmware-mediated.
- The nouveau project has reverse-engineered register specs for older GPU generations.
- CUDA is a proprietary runtime that requires NVIDIA's driver stack. Not portable to Isurus without NVIDIA's cooperation.

NVIDIA GPU support on Isurus would require either NVIDIA providing documentation/cooperation, or using reverse-engineered nouveau register specs (limited to older GPUs, incomplete for compute).

## Scope estimate

### Phase 1: AMD compute

Target: submit compute shaders to an AMD GPU from a ring 3 thread, read results back.

Work items:

- GPU PCI BAR mapping in kernel (extend existing `map_mmio_range`).
- GPU identification and initialization (read ASIC registers, configure memory controller).
- Command ring setup and PM4 packet builder.
- GPUVM page table management.
- Fence polling for completion.
- Userspace library API: `gpu_init`, `gpu_alloc`, `gpu_submit`, `gpu_wait`.

Estimated effort: 2-3 months for basic compute dispatch. Reference: AMD's register headers and Mesa's radeonsi command submission code.

### Phase 2: Compute library (shader compilation)

Target: compile and dispatch compute shaders (similar to OpenCL or CUDA).

Work items:

- Shader compiler (port AMD's ACO backend from Mesa, or use LLVM AMDGPU backend).
- Dispatch interface for compute kernels.
- Memory management API (host-to-device, device-to-host transfers).

Estimated effort: 3-6 months. The shader compiler is the largest component.

## Why not a Linux compatibility layer

Porting Linux GPU drivers to Isurus via a compatibility layer (reimplementing Linux kernel APIs) is theoretically possible but practically inadvisable:

- The Linux DRM/KMS subsystem is approximately 50,000 lines of framework code. Individual GPU drivers (amdgpu) add 200,000+ lines on top.
- The driver depends on Linux memory management (TTM, GEM, shmem), workqueues, timers, interrupts, firmware loading, debugfs, sysfs, and more.
- Maintaining API compatibility across Linux kernel versions is an ongoing burden. FreeBSD's Linuxkpi project, which does exactly this, requires a dedicated team.
- The resulting architecture fights Isurus's design: it would reintroduce kernel-mediated I/O on the GPU path, defeating the purpose of direct device access.

The direct MMIO approach is less code, faster at runtime, and consistent with how Isurus handles every other device.

## Relationship to DPDK and SPDK

GPU support follows the same pattern as NIC and storage access:

| Device | Linux approach | Isurus approach |
|--------|---------------|-----------------|
| NIC | Kernel driver (e1000, ixgbe) + socket API | DPDK: kernel maps MMIO, userspace polls rings |
| Storage | Kernel driver (nvme) + block layer + filesystem | SPDK: kernel maps MMIO, userspace polls queues |
| GPU | Kernel driver (amdgpu) + DRM/KMS + ioctl | GPU library: kernel maps MMIO, userspace submits commands |

In each case, the kernel's role is PCI enumeration and MMIO mapping. The device library runs entirely in ring 3, polling for completion. All buffers are NUMA-local. No interrupts. No syscalls on the data path.
