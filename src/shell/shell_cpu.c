// CPU-topology shell command: sys.cpu.ls.

#include "shell_internal.h"
#include <kernel/cpu.h>
#include <arch/acpi.h>
#include <syscall.h>

void cmd_lscpu(void) {
    if (use_syscalls) {
        SysCpuInfo info;
        sys_cpu_info(&info);

        sh_print("CPU(s):             ");
        sh_print_dec(info.cpu_count);
        sh_print("\n");

        uint32 online = 0;
        for (uint32 i = 0; i < info.cpu_count; i++) {
            if (info.cpus[i].running) online++;
        }
        sh_print("Online CPU(s):      ");
        sh_print_dec(online);
        sh_print("\n");

        sh_print("LAPIC base:         ");
        sh_print_hex(info.lapic_base, "\n");
        sh_print("IOAPIC base:        ");
        sh_print_hex(info.ioapic_base, "\n");

        uint32 numa_nodes = info.numa_node_count;
        if (numa_nodes == 0) numa_nodes = 1;
        sh_print("\nNUMA node(s):       ");
        sh_print_dec(numa_nodes);
        sh_print("\n");

        sh_print("\nPer-CPU info:\n");
        for (uint32 i = 0; i < info.cpu_count; i++) {
            sh_print("  CPU ");
            sh_print_dec(i);
            sh_print("  LAPIC ");
            sh_print_dec(info.cpus[i].lapic_id);
            if (info.cpus[i].has_numa) {
                sh_print("  NUMA ");
                sh_print_dec(info.cpus[i].numa_node);
            }
            if (info.cpus[i].running)
                sh_print("  [online]");
            else
                sh_print("  [offline]");
            if (i == 0)
                sh_print("  (BSP)");
            sh_print("\n");
        }
    } else {
        sh_print("CPU(s):             ");
        sh_print_dec(cpu_count);
        sh_print("\n");

        uint32 online = 0;
        for (uint32 i = 0; i < cpu_count; i++) {
            if (percpu[i].running) online++;
        }
        sh_print("Online CPU(s):      ");
        sh_print_dec(online);
        sh_print("\n");

        sh_print("LAPIC base:         ");
        sh_print_hex(lapic_base_addr, "\n");
        sh_print("IOAPIC base:        ");
        sh_print_hex(ioapic_base_addr, "\n");

        uint32 numa_nodes = acpi_numa_node_count();
        if (numa_nodes == 0) numa_nodes = 1;
        sh_print("\nNUMA node(s):       ");
        sh_print_dec(numa_nodes);
        sh_print("\n");

        sh_print("\nPer-CPU info:\n");
        for (uint32 i = 0; i < cpu_count; i++) {
            uint32 node = 0;
            int has_node = acpi_cpu_to_node(percpu[i].lapic_id, &node);
            sh_print("  CPU ");
            sh_print_dec(i);
            sh_print("  LAPIC ");
            sh_print_dec(percpu[i].lapic_id);
            if (has_node) {
                sh_print("  NUMA ");
                sh_print_dec(node);
            }
            if (percpu[i].running)
                sh_print("  [online]");
            else
                sh_print("  [offline]");
            if (i == 0)
                sh_print("  (BSP)");
            sh_print("\n");
        }
    }
}
