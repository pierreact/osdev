#include <types.h>
#include <system.cpu.h>
#include <system.acpi.h>
#include <system.monitor.h>

PerCPU percpu[MAX_CPUS];

// AP stacks are declared in kmain.s BSS section
extern uint8 ap_stacks[MAX_CPUS][AP_STACK_SIZE];

// Zero all per-CPU structs, then fill in LAPIC IDs and stack pointers from ACPI data
void cpu_init(void) {
    for (uint32 i = 0; i < MAX_CPUS; i++) {
        percpu[i].lapic_id = 0;
        percpu[i].running = 0;
        percpu[i].stack_top = 0;
    }

    // Setup per-CPU data from ACPI info
    for (uint32 i = 0; i < cpu_count; i++) {
        percpu[i].lapic_id = cpu_lapic_ids[i];
        // Stack top = base + size (stack grows down)
        percpu[i].stack_top = (uint64)&ap_stacks[i][AP_STACK_SIZE];
    }

    // BSP is already running
    percpu[0].running = 1;

    kprint("CPU: Per-CPU structures initialized for ");
    kprint_dec(cpu_count);
    kprint(" CPUs\n");
}
