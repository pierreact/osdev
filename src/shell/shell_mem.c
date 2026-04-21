// Memory-related shell commands: sys.mem.info, sys.mem.test, sys.mem.free.

#include "shell_internal.h"
#include <arch/acpi.h>
#include <syscall.h>

extern uint8  data_counter_mmap_entries;
extern uint32 MEMMAP_START;
extern uint32 PML4T_LOCATION;
extern uint32 PAGING_LOCATION_END;
extern uint32 mem_amount[2];

void cmd_meminfo(void) {
    if (use_syscalls) {
        SysMemInfo info;
        sys_meminfo(&info);
        sh_print("Memory Information:\n");
        sh_print_hex(info.mmap_entries, " MMAP entries\n");
        sh_print_hex(info.paging_location_end, " Paging structures end\n");
    } else {
        sh_print("Memory Information:\n");
        sh_print_hex(data_counter_mmap_entries, " MMAP entries\n");
        sh_print_hex((long)&MEMMAP_START, " MEMMAP start address\n");
        sh_print_hex(PML4T_LOCATION, " PML4T location\n");
        sh_print_hex(PAGING_LOCATION_END, " Paging structures end\n");
    }
}

void cmd_memtest(void) {
    sh_print("Testing heap allocator...\n");

    void *p1 = sh_malloc(100);
    if (p1) {
        sh_print("Allocated block 1: ");
        sh_print_hex((uint64)p1, "\n");
    } else {
        sh_print("Failed to allocate block 1\n");
        return;
    }

    void *p2 = sh_malloc(200);
    if (p2) {
        sh_print("Allocated block 2: ");
        sh_print_hex((uint64)p2, "\n");
    } else {
        sh_print("Failed to allocate block 2\n");
        sh_free(p1);
        return;
    }

    uint32 *test = (uint32*)p1;
    *test = 0xDEADBEEF;
    sh_print("Write test: ");
    sh_print_hex(*test, "\n");

    sh_free(p1);
    sh_print("Freed block 1\n");

    void *p3 = sh_malloc(50);
    if (p3) {
        sh_print("Allocated block 3: ");
        sh_print_hex((uint64)p3, "\n");
    }

    sh_free(p2);
    sh_free(p3);
    sh_print("Test complete\n");
}

void cmd_free(void) {
    uint32 used, free_pages, total;
    sh_heap_stats(&used, &free_pages, &total);

    uint64 total_bytes, total_mb;
    uint32 numa_nodes;
    uint64 paging_end;

    if (use_syscalls) {
        SysMemInfo minfo;
        sys_meminfo(&minfo);
        total_bytes = (minfo.mem_amount_high << 32) | minfo.mem_amount_low;
        paging_end = minfo.paging_location_end;
        SysCpuInfo cinfo;
        sys_cpu_info(&cinfo);
        numa_nodes = cinfo.numa_node_count;
    } else {
        total_bytes = ((uint64)mem_amount[0] << 32) | (uint64)mem_amount[1];
        paging_end = (uint64)PAGING_LOCATION_END;
        numa_nodes = acpi_numa_node_count();
    }

    total_mb = total_bytes / (1024 * 1024);
    uint32 heap_used_kb = used * 4;
    uint32 heap_free_kb = free_pages * 4;
    uint32 heap_total_kb = total * 4;

    if (numa_nodes == 0) numa_nodes = 1;
    uint64 per_node_mb = total_mb / numa_nodes;

    uint64 kernel_kb = paging_end / 1024;
    uint64 node0_used_kb = kernel_kb + heap_used_kb;
    uint64 node0_free_mb = per_node_mb - (node0_used_kb / 1024);

    sh_print("              total       used       free\n");

    sh_print("Cluster ");
    sh_print_dec_pad(total_mb, 10);
    sh_print(" MB");
    sh_print_dec_pad(node0_used_kb / 1024, 9);
    sh_print(" MB");
    sh_print_dec_pad(total_mb - (node0_used_kb / 1024), 9);
    sh_print(" MB\n");

    sh_print("Node    ");
    sh_print_dec_pad(total_mb, 10);
    sh_print(" MB");
    sh_print_dec_pad(node0_used_kb / 1024, 9);
    sh_print(" MB");
    sh_print_dec_pad(total_mb - (node0_used_kb / 1024), 9);
    sh_print(" MB\n");

    sh_print("NUMA 0  ");
    sh_print_dec_pad(per_node_mb, 10);
    sh_print(" MB");
    sh_print_dec_pad(node0_used_kb / 1024, 9);
    sh_print(" MB");
    sh_print_dec_pad(node0_free_mb, 9);
    sh_print(" MB\n");

    if (numa_nodes > 1) {
        sh_print("NUMA 1  ");
        sh_print_dec_pad(per_node_mb, 10);
        sh_print(" MB");
        sh_print_dec_pad(0, 9);
        sh_print(" MB");
        sh_print_dec_pad(per_node_mb, 9);
        sh_print(" MB\n");
    }

    sh_print("Heap    ");
    sh_print_dec_pad(heap_total_kb, 10);
    sh_print(" KB");
    sh_print_dec_pad(heap_used_kb, 9);
    sh_print(" KB");
    sh_print_dec_pad(heap_free_kb, 9);
    sh_print(" KB\n");
}
