// Isurus Demo App
//
// First userland application for Isurus. Runs in ring 3 on each
// application core. Each thread reads its own ThreadMeta and reports
// CPU index, NUMA node, and assigned NIC information.
//
// This is a template for how to write Isurus applications. Future
// extensions will demonstrate DPDK polling, SPDK storage access,
// and cross-NUMA shared memory patterns.

#include "../libc/isurus.h"
#include "../libc/stdio.h"
#include "../libc/string.h"

// Entry point. The kernel drops each AP to ring 3 at _start.
// ThreadMeta pointer will be passed in RDI by the kernel.
void _start(void) {
    // TODO: When the kernel implements AP ring 3 dispatch, ThreadMeta
    // will be at a fixed VA or passed as an argument. For now, this
    // is a placeholder that prints a message and exits.

    puts("=== Isurus Demo App ===\n");
    puts("Running in ring 3.\n");

    // Future: read ThreadMeta, print per-core info
    // ThreadMeta *meta = (ThreadMeta *)THREAD_META_VA;
    // puts("CPU "); print_dec(meta->cpu_index);
    // puts("  NUMA "); print_dec(meta->numa_node);
    // ...

    exit();
}
