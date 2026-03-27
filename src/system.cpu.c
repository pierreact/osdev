#include <types.h>
#include <system.cpu.h>
#include <system.acpi.h>
#include <system.mem.h>
#include <system.monitor.h>

PerCPU percpu[MAX_CPUS];

// Zero all per-CPU structs, then fill in LAPIC IDs and stack pointers from ACPI data
void cpu_init(void) {
    for (uint32 i = 0; i < MAX_CPUS; i++) {
        percpu[i].lapic_id = 0;
        percpu[i].running = 0;
        percpu[i].stack_top = 0;
    }

    // Setup per-CPU data from ACPI info
    // AP stacks allocated from high memory (above 1MB, via PAGING_LOCATION_END)
    for (uint32 i = 0; i < cpu_count; i++) {
        percpu[i].lapic_id = cpu_lapic_ids[i];
        if (i > 0) {
            uint64 base = alloc_pages(AP_STACK_SIZE / 4096);
            percpu[i].stack_top = base + AP_STACK_SIZE;
        }
    }

    // BSP is already running (BSP stack allocated separately in init sequence)
    percpu[0].running = 1;

    kprint("CPU: Per-CPU structures initialized for ");
    kprint_dec(cpu_count);
    kprint(" CPUs\n");
}
