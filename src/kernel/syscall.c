#include <types.h>
#include <syscall.h>
#include <drivers/monitor.h>
#include <kernel/mem.h>
#include <drivers/ide.h>
#include <fs/fat32.h>
#include <arch/acpi.h>
#include <kernel/cpu.h>
#include <kernel/task.h>
#include <fs/iso9660.h>
#include <fs/vfs.h>
#include <arch/acpi.h>

// Forward declaration - cmd_reboot stays in shell/shell.c for now
// but is called from ring 0 via syscall
extern void cmd_reboot(void);

// Input ring buffer (written by ISR, read by syscall)
#define INPUT_BUF_SIZE 64
static volatile char input_buf[INPUT_BUF_SIZE];
static volatile uint32 input_head;
static volatile uint32 input_tail;

// Shell task ID (set during shell task creation)
uint8 shell_task_id = 0;

void input_buf_push(char c) {
    uint32 next = (input_head + 1) % INPUT_BUF_SIZE;
    if (next != input_tail) {
        input_buf[input_head] = c;
        input_head = next;
    }
    // Wake the shell task if it was waiting for input
    task_wake(shell_task_id);
}

static int input_buf_pop(void) {
    if (input_tail == input_head)
        return 0;
    char c = input_buf[input_tail];
    input_tail = (input_tail + 1) % INPUT_BUF_SIZE;
    return (int)(unsigned char)c;
}

// Extern symbols for SYS_MEMINFO
extern uint8 data_counter_mmap_entries;
extern uint32 PAGING_LOCATION_END;
extern uint32 mem_amount[2];

// Syscall handlers
static long sys_handle_putc(uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    putc((char)a1);
    return 0;
}

static long sys_handle_kprint(uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    kprint((char *)a1);
    return 0;
}

static long sys_handle_cls(uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    cls();
    return 0;
}

static long sys_handle_heap_stats(uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5) {
    (void)a4; (void)a5;
    heap_stats((uint32 *)a1, (uint32 *)a2, (uint32 *)a3);
    return 0;
}

static long sys_handle_kmalloc(uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    return (long)kmalloc((size_t)a1);
}

static long sys_handle_kfree(uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    kfree((void *)a1);
    return 0;
}

static long sys_handle_ide_read(uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5) {
    (void)a4; (void)a5;
    return ide_read_sectors((uint32)a1, (uint8)a2, (uint8 *)a3);
}

static long sys_handle_ide_write(uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5) {
    (void)a3; (void)a4; (void)a5;
    return ide_write_sector((uint32)a1, (uint8 *)a2);
}

static long sys_handle_ide_model(uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return (long)ide_get_model();
}

static long sys_handle_ide_sectors(uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return (long)ide_get_sector_count();
}

static long sys_handle_fat32_ls(uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    fat32_list_root();
    return 0;
}

static long sys_handle_fat32_cat(uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    return fat32_cat_file((const char *)a1);
}

static long sys_handle_acpi_ls(uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    acpi_lsacpi();
    return 0;
}

static long sys_handle_reboot(uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    cmd_reboot();
    return 0;
}

static long sys_handle_cpu_info(uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    SysCpuInfo *info = (SysCpuInfo *)a1;
    info->cpu_count = cpu_count;
    info->lapic_base = lapic_base_addr;
    info->ioapic_base = ioapic_base_addr;
    info->numa_node_count = acpi_numa_node_count();
    for (uint32 i = 0; i < cpu_count && i < MAX_SYS_CPUS; i++) {
        info->cpus[i].lapic_id = percpu[i].lapic_id;
        info->cpus[i].running = percpu[i].running;
        info->cpus[i].in_usermode = percpu[i].in_usermode;
        uint32 node = 0;
        info->cpus[i].has_numa = acpi_cpu_to_node(percpu[i].lapic_id, &node) ? 1 : 0;
        info->cpus[i].numa_node = node;
    }
    return 0;
}

static long sys_handle_meminfo(uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    SysMemInfo *info = (SysMemInfo *)a1;
    info->paging_location_end = (uint64)PAGING_LOCATION_END;
    info->mem_amount_high = mem_amount[0];
    info->mem_amount_low = mem_amount[1];
    info->mmap_entries = data_counter_mmap_entries;
    return 0;
}

static long sys_handle_kprint_dec(uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    kprint_dec(a1);
    return 0;
}

static long sys_handle_kprint_hex(uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5) {
    (void)a3; (void)a4; (void)a5;
    kprint_long2hex((long)a1, (char *)a2);
    return 0;
}

static long sys_handle_kprint_decpad(uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5) {
    (void)a3; (void)a4; (void)a5;
    kprint_dec_pad(a1, (uint32)a2);
    return 0;
}

static long sys_handle_readchar(uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return input_buf_pop();
}

static long sys_handle_wait_input(uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    int c;
    while ((c = input_buf_pop()) == 0) {
        __asm__ volatile("sti; hlt; cli");
    }
    return c;
}

static long sys_handle_yield(uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    task_yield();
    return 0;
}

static long sys_handle_task_exit(uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    task_exit();
    return 0;
}

static long sys_handle_iso_ls(uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    vfs_ls((const char *)a1);
    return 0;
}

static long sys_handle_iso_read(uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5) {
    (void)a4; (void)a5;
    return (long)vfs_read_file((const char *)a1, (void *)a2, (uint32)a3);
}

// AP test: function dispatched to each AP, returns cpu index
static uint64 ap_test_fn(uint64 arg) {
    (void)arg;
    // Just return a magic value to prove we ran
    return 0xA000 + arg;
}

static long sys_handle_test_ap(uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    kprint("AP test: dispatching to all APs...\n");
    for (uint32 i = 1; i < cpu_count; i++) {
        if (!percpu[i].running) continue;
        uint64 result = ap_dispatch(i, ap_test_fn, i);
        kprint("  CPU ");
        kprint_dec(i);
        kprint(": result=");
        kprint_long2hex(result, "\n");
    }
    kprint("AP test: done\n");
    return 0;
}

// Syscall table
typedef long (*syscall_fn)(uint64, uint64, uint64, uint64, uint64);

static syscall_fn syscall_table[SYS_NR_MAX] = {
    [SYS_PUTC]        = sys_handle_putc,
    [SYS_KPRINT]      = sys_handle_kprint,
    [SYS_CLS]         = sys_handle_cls,
    [SYS_HEAP_STATS]  = sys_handle_heap_stats,
    [SYS_KMALLOC]     = sys_handle_kmalloc,
    [SYS_KFREE]       = sys_handle_kfree,
    [SYS_IDE_READ]    = sys_handle_ide_read,
    [SYS_IDE_WRITE]   = sys_handle_ide_write,
    [SYS_IDE_MODEL]   = sys_handle_ide_model,
    [SYS_IDE_SECTORS] = sys_handle_ide_sectors,
    [SYS_FAT32_LS]    = sys_handle_fat32_ls,
    [SYS_FAT32_CAT]   = sys_handle_fat32_cat,
    [SYS_ACPI_LS]     = sys_handle_acpi_ls,
    [SYS_REBOOT]      = sys_handle_reboot,
    [SYS_CPU_INFO]    = sys_handle_cpu_info,
    [SYS_MEMINFO]     = sys_handle_meminfo,
    [SYS_KPRINT_DEC]  = sys_handle_kprint_dec,
    [SYS_KPRINT_HEX]  = sys_handle_kprint_hex,
    [SYS_KPRINT_DECPAD] = sys_handle_kprint_decpad,
    [SYS_READCHAR]    = sys_handle_readchar,
    [SYS_WAIT_INPUT]  = sys_handle_wait_input,
    [SYS_YIELD]       = sys_handle_yield,
    [SYS_TASK_EXIT]   = sys_handle_task_exit,
    [SYS_ISO_LS]      = sys_handle_iso_ls,
    [SYS_ISO_READ]    = sys_handle_iso_read,
    [SYS_TEST_AP]     = sys_handle_test_ap,
};

long syscall_dispatch(uint64 nr, uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5) {
    if (nr >= SYS_NR_MAX || !syscall_table[nr])
        return -1;
    return syscall_table[nr](a1, a2, a3, a4, a5);
}
