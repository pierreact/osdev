#include <kernel/loader.h>
#include <kernel/cpu.h>
#include <kernel/tss.h>
#include <kernel/mem.h>
#include <fs/vfs.h>
#include <drivers/monitor.h>
#include <arch/acpi.h>

// Per-AP state for ring 3 execution
uint64 ap_user_stacks[MAX_CPUS];
uint64 ap_entry_addrs[MAX_CPUS];

// AP ring 3 launcher. Called on each AP via ap_dispatch.
// Does IRETQ to ring 3 -- does NOT return. When the ring 3 code
// calls exit (SYS_TASK_EXIT), the syscall handler resets the AP's
// stack and enters a new park loop. BSP sees done=1.
uint64 ap_run_ring3(uint64 cpu_idx) {
    uint32 idx = (uint32)cpu_idx;

    uint64 user_stack_top = ap_user_stacks[idx] + USER_STACK_SIZE;
    uint64 entry_addr = ap_entry_addrs[idx];
    uint64 meta_ptr = (uint64)&thread_meta[idx];
    percpu[idx].in_usermode = 1;

    __asm__ volatile(
        "cli\n"
        "mov %0, %%rax\n"       // user RSP
        "mov %1, %%rbx\n"       // entry point
        "mov %2, %%rdi\n"       // _start(ThreadMeta *meta)
        "push $0x23\n"          // SS (ring 3 data)
        "push %%rax\n"          // RSP
        "push $0x202\n"         // RFLAGS (IF=1)
        "push $0x2B\n"          // CS (ring 3 code 64-bit)
        "push %%rbx\n"          // RIP
        "iretq\n"
        :
        : "r"(user_stack_top), "r"(entry_addr), "r"(meta_ptr)
        : "rax", "rbx", "rdi", "memory"
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

    // Allocate user stacks and set entry address for each AP
    for (uint32 i = 1; i < cpu_count; i++) {
        if (!percpu[i].running) continue;
        ap_user_stacks[i] = alloc_pages(USER_STACK_SIZE / 4096);
        ap_entry_addrs[i] = USER_LOAD_ADDR;
    }

    // Set up ring 3 on each AP (sequentially, shared TSS descriptor)
    for (uint32 i = 1; i < cpu_count; i++) {
        if (!percpu[i].running) continue;
        ap_dispatch(i, ap_setup_ring3, i);
    }

    kprint("LOADER: launching on APs...\n");

    // Dispatch ring 3 entry to each AP with its own CPU index.
    // Sequential because each AP needs its own jmpbuf and percpu.
    for (uint32 i = 1; i < cpu_count; i++) {
        if (!percpu[i].running) continue;
        kprint("LOADER: dispatching to CPU ");
        kprint_dec(i);
        kprint("...\n");
        ap_dispatch(i, ap_run_ring3, i);
        kprint("LOADER: CPU ");
        kprint_dec(i);
        kprint(" done\n");
    }

    kprint("LOADER: all APs done\n");
    return 0;
}
