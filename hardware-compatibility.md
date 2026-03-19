# Hardware Compatibility

## Intel processors only

This OS targets Intel processors, or any CPU with cache-coherent DMA. This is a hard requirement.

## Cache-coherent DMA

When a NIC DMAs packets into memory, the CPU cache may hold stale data at those addresses. On Intel hardware, the cache controller snoops DMA transactions automatically: when the NIC writes to a physical address, any cached copy of that line is invalidated or updated before the CPU reads it. This is cache-coherent DMA.

The consequence: DMA buffers can live in normal write-back cacheable memory. Threads reading packet data get full cache performance with no manual coherence management.

Without cache-coherent DMA (e.g., some ARM or embedded platforms), the OS would need to explicitly invalidate or flush cache lines around every DMA buffer before reading. On the receive path, that means a cache invalidation on every incoming packet before the thread can read it. On a polling-based NIC driver processing millions of packets per second, this adds latency to every iteration of the hot path.

This is not a soft preference. Non-coherent DMA hardware is not supported.
