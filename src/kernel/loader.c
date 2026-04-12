#include <kernel/loader.h>
#include <kernel/cpu.h>
#include <kernel/tss.h>
#include <kernel/mem.h>
#include <fs/vfs.h>
#include <drivers/monitor.h>
#include <arch/acpi.h>

// Per-AP state for ring 3 execution
static uint64 ap_user_stacks[MAX_CPUS];

// setjmp buffer per AP: when ring 3 code does SYSCALL(SYS_TASK_EXIT),
// the handler longjmps back here so ap_run_ring3 returns normally
// to the trampoline park loop.
void *ap_jmpbuf[MAX_CPUS][5];

// AP ring 3 launcher. Called on each AP via ap_dispatch.
// Uses setjmp to save the return point, then IRETQ to ring 3.
// When the ring 3 code calls exit (SYS_TASK_EXIT), the syscall
// handler does longjmp back here.
static uint64 ap_run_ring3(uint64 cpu_idx) {
    uint32 idx = (uint32)cpu_idx;

    if (__builtin_setjmp(ap_jmpbuf[idx]) != 0) {
        // Returned via longjmp from sys_handle_task_exit
        percpu[idx].in_usermode = 0;
        return 0;
    }

    // First time: drop to ring 3
    uint64 user_stack_top = ap_user_stacks[idx] + USER_STACK_SIZE;
    uint64 entry = USER_LOAD_ADDR;
    percpu[idx].in_usermode = 1;

    // IRETQ to ring 3
    __asm__ volatile(
        "cli\n"
        "push $0x23\n"          // SS (ring 3 data)
        "push %0\n"             // RSP
        "push $0x202\n"         // RFLAGS (IF=1)
        "push $0x2B\n"          // CS (ring 3 code 64-bit)
        "push %1\n"             // RIP (entry point)
        "iretq\n"
        :
        : "r"(user_stack_top), "r"(entry)
        : "memory"
    );

    __builtin_unreachable();
}

int loader_exec(const char *iso_path) {
    uint32 file_size = vfs_file_size(iso_path);
    if (file_size == 0) {
        kprint("LOADER: file not found: ");
        kprint((char *)iso_path);
        kprint("\n");
        return -1;
    }

    // Load the binary at USER_LOAD_ADDR (identity-mapped by boot paging)
    if (vfs_read_file(iso_path, (void *)USER_LOAD_ADDR, file_size) < 0) {
        kprint("LOADER: failed to read binary\n");
        return -1;
    }

    kprint("LOADER: loaded ");
    kprint((char *)iso_path);
    kprint(" (");
    kprint_dec(file_size);
    kprint(" bytes) at ");
    kprint_long2hex(USER_LOAD_ADDR, "\n");

    // Allocate user stacks for each AP (once)
    for (uint32 i = 1; i < cpu_count; i++) {
        if (!percpu[i].running) continue;
        if (ap_user_stacks[i] == 0)
            ap_user_stacks[i] = alloc_pages(USER_STACK_SIZE / 4096);
    }

    // Set up ring 3 on each AP (sequentially — shared TSS descriptor)
    for (uint32 i = 1; i < cpu_count; i++) {
        if (!percpu[i].running) continue;
        ap_dispatch(i, ap_setup_ring3, i);
    }

    kprint("LOADER: launching on APs...\n");

    // Dispatch ring 3 entry to each AP with its own CPU index.
    // Sequential because each AP needs its own jmpbuf and percpu.
    for (uint32 i = 1; i < cpu_count; i++) {
        if (!percpu[i].running) continue;
        ap_dispatch(i, ap_run_ring3, i);
    }

    kprint("LOADER: all APs done\n");
    return 0;
}
