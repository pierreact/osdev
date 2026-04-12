// Isurus Demo App
//
// Runs in ring 3 on each application core. The kernel passes a pointer
// to this core's ThreadMeta in RDI (_start's first argument).
// Prints CPU index, NUMA node, and assigned NIC information.
//
// This is a template for how to write Isurus applications. Future
// extensions will demonstrate DPDK polling, SPDK storage access,
// and cross-NUMA shared memory patterns.

#include "../libc/isurus.h"
#include "../libc/stdio.h"

static void print_mac(uint8 *mac) {
    for (int i = 0; i < 6; i++) {
        print_hex8(mac[i]);
        if (i < 5) putc(':');
    }
}

void _start(ThreadMeta *meta) {
    puts("  CPU ");
    print_dec(meta->cpu_index);
    puts("  NUMA ");
    if (meta->numa_node == THREAD_NUMA_UNKNOWN)
        puts("-");
    else
        print_dec(meta->numa_node);

    puts("  NIC ");
    if (meta->nic_index == NIC_NONE) {
        puts("(none)");
    } else {
        print_hex8(meta->nic_bus);
        putc(':');
        print_hex8(meta->nic_dev);
        putc('.');
        print_dec(meta->nic_func);
        puts("  MAC ");
        print_mac(meta->nic_mac);
    }

    putc('\n');
    exit();
}
