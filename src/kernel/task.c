#include <types.h>
#include <kernel/task.h>
#include <kernel/tss.h>
#include <kernel/cpu.h>
#include <kernel/mem.h>
#include <drivers/monitor.h>

static Task bsp_tasks[MAX_BSP_TASKS];
uint8 current_task_id = 0;

static uint8 find_next_ready(void) {
    for (uint8 i = 1; i <= MAX_BSP_TASKS; i++) {
        uint8 idx = (current_task_id + i) % MAX_BSP_TASKS;
        if (bsp_tasks[idx].state == TASK_READY)
            return idx;
    }
    return current_task_id;
}

// Trampoline for first-time task launch.
// Called when task_yield switches to a new task's kernel stack for the first time.
// The new task has never run, so we drop to ring 3 via IRETQ.
static void task_launch_trampoline(void) {
    Task *t = &bsp_tasks[current_task_id];

    // Set percpu user_rsp for the SYSRET/IRETQ path
    percpu[0].user_rsp = t->user_rsp;

    // Drop to ring 3 via IRETQ
    // No swapgs here: KERNEL_GS_BASE already holds &percpu[0] from syscall_init.
    // syscall_entry's swapgs will swap it into GS_BASE on first SYSCALL.
    __asm__ volatile(
        "cli\n\t"
        "pushq $0x23\n\t"           // SS  = ring 3 data (0x20 | RPL=3)
        "pushq %0\n\t"             // RSP = user stack
        "pushq $0x202\n\t"         // RFLAGS (IF=1)
        "pushq $0x2B\n\t"          // CS  = ring 3 code (0x28 | RPL=3)
        "pushq %1\n\t"             // RIP = task entry point
        "iretq\n\t"
        :
        : "r"(t->user_rsp), "r"(t->entry)
        : "memory"
    );
    __builtin_unreachable();
}

int task_create(const char *name, void (*entry)(void)) {
    uint8 slot = 0xFF;
    for (uint8 i = 0; i < MAX_BSP_TASKS; i++) {
        if (bsp_tasks[i].state == TASK_UNUSED) {
            slot = i;
            break;
        }
    }
    if (slot == 0xFF) return -1;

    Task *t = &bsp_tasks[slot];
    t->id = slot;
    t->state = TASK_READY;
    t->cr3 = percpu[0].cr3;

    for (int i = 0; i < 15 && name[i]; i++)
        t->name[i] = name[i];
    t->name[15] = '\0';

    // Allocate user stack (ring 3)
    t->stack_base = alloc_pages(TASK_STACK_PAGES);
    t->user_rsp = t->stack_base + TASK_STACK_PAGES * 4096;

    // Allocate kernel stack (ring 0)
    t->kstack_base = alloc_pages(TASK_STACK_PAGES);
    t->kstack_top = t->kstack_base + TASK_STACK_PAGES * 4096;

    t->entry = (uint64)entry;

    // Build initial kernel stack so task_yield can switch to this task.
    // task_yield's inline asm pushes rbx,rbp,r12,r13,r14,r15 (6 regs)
    // then saves RSP. On restore it pops those 6 regs and does "ret".
    // The "ret" address must be task_launch_trampoline.
    volatile uint64 *ksp = (volatile uint64 *)(t->kstack_top);
    ksp -= 7;               // 6 callee-saved regs + return address
    ksp[6] = (uint64)task_launch_trampoline;  // return address
    ksp[5] = 0;             // rbx
    ksp[4] = 0;             // rbp
    ksp[3] = 0;             // r12
    ksp[2] = 0;             // r13
    ksp[1] = 0;             // r14
    ksp[0] = 0;             // r15
    t->kernel_rsp = (uint64)ksp;

    kprint("TASK: created '");
    kprint(t->name);
    kprint("' (id=");
    kprint_dec(slot);
    kprint(")\n");

    return slot;
}

void task_yield(void) {
    uint8 next = find_next_ready();
    if (next == current_task_id) {
        if (bsp_tasks[current_task_id].state == TASK_WAITING) {
            __asm__ volatile("sti; hlt; cli");
            next = find_next_ready();
            if (next == current_task_id)
                return;
        } else {
            return;
        }
    }

    Task *cur = &bsp_tasks[current_task_id];
    Task *nxt = &bsp_tasks[next];

    if (cur->state == TASK_RUNNING)
        cur->state = TASK_READY;
    nxt->state = TASK_RUNNING;

    tss_set_rsp0(0, nxt->kstack_top);
    percpu[0].user_rsp = nxt->user_rsp;

    uint8 prev_id = current_task_id;
    current_task_id = next;

    // Switch kernel stacks.
    // Push callee-saved regs, save RSP, load new RSP, pop callee-saved regs, ret.
    // For a running task being switched out, "ret" returns into the caller of
    // task_yield (i.e., back into syscall_dispatch -> syscall_entry -> SYSRET).
    // For a new task, "ret" goes to task_launch_trampoline -> IRETQ to ring 3.
    uint64 *save_rsp = &bsp_tasks[prev_id].kernel_rsp;
    uint64 load_rsp = nxt->kernel_rsp;

    __asm__ volatile(
        "pushq %%rbx\n\t"
        "pushq %%rbp\n\t"
        "pushq %%r12\n\t"
        "pushq %%r13\n\t"
        "pushq %%r14\n\t"
        "pushq %%r15\n\t"
        "movq %%rsp, (%0)\n\t"
        "movq %1, %%rsp\n\t"
        "popq %%r15\n\t"
        "popq %%r14\n\t"
        "popq %%r13\n\t"
        "popq %%r12\n\t"
        "popq %%rbp\n\t"
        "popq %%rbx\n\t"
        :
        : "r"(save_rsp), "r"(load_rsp)
        : "memory"
    );
}

void task_exit(void) {
    bsp_tasks[current_task_id].state = TASK_UNUSED;
    kprint("TASK: '");
    kprint(bsp_tasks[current_task_id].name);
    kprint("' exited\n");
    task_yield();
    for (;;) __asm__ volatile("hlt");
}

void task_wake(uint8 task_id) {
    if (task_id < MAX_BSP_TASKS && bsp_tasks[task_id].state == TASK_WAITING)
        bsp_tasks[task_id].state = TASK_READY;
}

void task_init(void) {
    for (uint8 i = 0; i < MAX_BSP_TASKS; i++)
        bsp_tasks[i].state = TASK_UNUSED;

    kprint("TASK: BSP task scheduler initialized\n");
}

// Start the first task (task 0) by dropping to ring 3. Does not return.
void task_run_first(void) {
    Task *t = &bsp_tasks[0];
    if (t->state == TASK_UNUSED) {
        kprint("TASK: no task to run\n");
        for (;;) __asm__ volatile("hlt");
    }

    t->state = TASK_RUNNING;
    current_task_id = 0;

    // Set TSS RSP0 to this task's kernel stack (for hardware interrupts)
    tss_set_rsp0(0, t->kstack_top);

    // Set percpu kernel stack (for SYSCALL entry stack switch)
    percpu[0].stack_top = t->kstack_top;
    percpu[0].user_stack_top = t->user_rsp;
    percpu[0].user_rsp = t->user_rsp;
    percpu[0].in_usermode = 1;

    kprint("TASK: dropping to ring 3 ('");
    kprint(t->name);
    kprint("')\n");

    // Drop to ring 3 via IRETQ
    // No swapgs: KERNEL_GS_BASE already holds &percpu[0] from syscall_init.
    // syscall_entry's swapgs will swap it into GS_BASE on first SYSCALL.
    __asm__ volatile(
        "cli\n\t"
        "pushq $0x23\n\t"           // SS  = ring 3 data (0x20 | RPL=3)
        "pushq %0\n\t"             // RSP = user stack
        "pushq $0x202\n\t"         // RFLAGS (IF=1)
        "pushq $0x2B\n\t"          // CS  = ring 3 code (0x28 | RPL=3)
        "pushq %1\n\t"             // RIP = task entry point
        "iretq\n\t"
        :
        : "r"(t->user_rsp), "r"(t->entry)
        : "memory"
    );
    __builtin_unreachable();
}
