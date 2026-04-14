# Hardware Compatibility

## Processor requirement: cache-coherent DMA

This OS requires processors with cache-coherent DMA. To our knowledge, Intel is currently the only brand providing DMA cache-coherent CPUs. Any processor that supports cache-coherent DMA should work.

## What is cache-coherent DMA

When a NIC DMAs packets into memory, the CPU cache may hold stale data at those addresses. On cache-coherent hardware (e.g. Intel), the cache controller snoops DMA transactions automatically: when the NIC writes to a physical address, any cached copy of that line is invalidated or updated before the CPU reads it.

The consequence: DMA buffers can live in normal write-back cacheable memory. Threads reading packet data get full cache performance with no manual coherence management.

## Why Isurus requires it

This is not a correctness issue. A non-coherent system would still work with proper cache management. The requirement comes from Isurus's specific design choices:

**Performance at line rate.** Without coherent DMA, every received packet requires explicit cache line invalidations before the CPU can read the DMA data. A 2KB packet buffer spans 32 cache lines (64 bytes each) -- that is 32 `clflush` instructions per packet. At millions of packets per second on a polling-based NIC driver, this adds significant latency to every iteration of the hot path.

**Zero-copy design.** Isurus's I/O path is zero-copy: the DMA buffer is the application buffer. There is no intermediate kernel buffer or bounce buffer. Without coherent DMA, you would either flush before every read (adding per-packet latency) or copy through uncacheable memory (breaking zero-copy and adding a memcpy).

**Userspace direct access.** Isurus maps device MMIO and DMA buffers directly into ring 3 (userspace). Some cache management instructions (`wbinvd`) are ring 0 only and cannot be executed from userspace. `clflush` is available in ring 3 but adds per-cache-line overhead to the userspace hot path. The DPDK library would need explicit cache management on every TX/RX operation, complicating the code and the data path.

A kernel that uses bounce buffers or kernel-managed DMA could work on non-coherent hardware. But that is the opposite of what Isurus does.

## Storage controller: AHCI required

SATA controllers must be in AHCI mode. IDE compatibility mode is not supported. AHCI is the default on all modern hardware (post-2012), QEMU Q35, and cloud instances. If your BIOS is set to "IDE" or "Legacy" SATA mode, switch it to "AHCI" before booting Isurus.
