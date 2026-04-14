#include <types.h>
#include <kernel/cpu.h>
#include <arch/acpi.h>
#include <kernel/mem.h>
#include <drivers/monitor.h>

// Verify struct size matches assembly constant
_Static_assert(sizeof(PerCPU) == PERCPU_SIZE,
    "PerCPU size mismatch: update PERCPU_SIZE and ap_trampoline.asm");

PerCPU percpu[MAX_CPUS];
ThreadMeta thread_meta[MAX_CPUS];
APWork ap_work[MAX_CPUS];

extern uint32 PML4T_LOCATION;

// Zero all per-CPU structs, then fill in LAPIC IDs and stack pointers from ACPI data
void cpu_init(void) {
    for (uint32 i = 0; i < MAX_CPUS; i++) {
        percpu[i].lapic_id = 0;
        percpu[i].running = 0;
        percpu[i].in_usermode = 0;
        percpu[i].stack_top = 0;
        percpu[i].user_stack_top = 0;
        percpu[i].user_rsp = 0;
        percpu[i].cr3 = 0;
    }

    // Setup per-CPU data from ACPI info
    // AP stacks allocated from high memory (above 1MB, via PAGING_LOCATION_END)
    for (uint32 i = 0; i < cpu_count; i++) {
        percpu[i].lapic_id = cpu_lapic_ids[i];
        percpu[i].cr3 = (uint64)PML4T_LOCATION;
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

ThreadMeta *thread_meta_get(uint32 cpu_idx) {
    if (cpu_idx >= MAX_CPUS) return NULL;
    return &thread_meta[cpu_idx];
}

_Static_assert(sizeof(APWork) == APWORK_SIZE,
    "APWork size mismatch: update APWORK_SIZE and ap_trampoline.asm");

uint64 ap_dispatch(uint32 cpu_idx, uint64 (*fn)(uint64 arg), uint64 arg) {
    if (cpu_idx == 0 || cpu_idx >= cpu_count) return (uint64)-1;
    if (!percpu[cpu_idx].running) return (uint64)-1;

    APWork *w = &ap_work[cpu_idx];
    w->fn = (uint64)fn;
    w->arg = arg;
    w->result = 0;
    w->done = 0;

    // Memory barrier before signaling ready
    __asm__ volatile("" ::: "memory");
    w->ready = 1;

    // Poll for completion
    while (!w->done) {
        __asm__ volatile("pause" ::: "memory");
    }

    return w->result;
}

void ap_dispatch_all(uint64 (*fn)(uint64 arg), uint64 arg) {
    // Signal all APs
    for (uint32 i = 1; i < cpu_count; i++) {
        if (!percpu[i].running) continue;
        APWork *w = &ap_work[i];
        w->fn = (uint64)fn;
        w->arg = arg;
        w->result = 0;
        w->done = 0;
    }
    __asm__ volatile("" ::: "memory");
    for (uint32 i = 1; i < cpu_count; i++) {
        if (!percpu[i].running) continue;
        ap_work[i].ready = 1;
    }

    // Wait for all to complete
    for (uint32 i = 1; i < cpu_count; i++) {
        if (!percpu[i].running) continue;
        while (!ap_work[i].done) {
            __asm__ volatile("pause" ::: "memory");
        }
    }
}
