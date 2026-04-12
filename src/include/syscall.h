#ifndef SYSCALL_H
#define SYSCALL_H

#include <types.h>

// Syscall numbers
#define SYS_PUTC          0
#define SYS_KPRINT        1
#define SYS_CLS           2
#define SYS_HEAP_STATS    3
#define SYS_KMALLOC       4
#define SYS_KFREE         5
#define SYS_IDE_READ      6
#define SYS_IDE_WRITE     7
#define SYS_IDE_MODEL     8
#define SYS_IDE_SECTORS   9
#define SYS_FAT32_LS      10
#define SYS_FAT32_CAT     11
#define SYS_ACPI_LS       12
#define SYS_REBOOT        13
#define SYS_CPU_INFO      14
#define SYS_MEMINFO       15
#define SYS_KPRINT_DEC    16
#define SYS_KPRINT_HEX    17
#define SYS_KPRINT_DECPAD 18
#define SYS_READCHAR      19
#define SYS_WAIT_INPUT    20
#define SYS_YIELD         21
#define SYS_TASK_EXIT     22
#define SYS_ISO_LS        23
#define SYS_ISO_READ      24
#define SYS_TEST_AP       25
#define SYS_NR_MAX        26

// User-side syscall wrappers
static inline long sys_syscall0(long nr) {
    long ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(nr) : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_syscall1(long nr, long a1) {
    long ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(nr), "D"(a1) : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_syscall2(long nr, long a1, long a2) {
    long ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(nr), "D"(a1), "S"(a2) : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_syscall3(long nr, long a1, long a2, long a3) {
    long ret;
    register long r10 __asm__("r10") = a3;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(nr), "D"(a1), "S"(a2), "r"(r10) : "rcx", "r11", "memory");
    return ret;
}

// Convenience wrappers
static inline void sys_putc(char c)              { sys_syscall1(SYS_PUTC, c); }
static inline void sys_kprint(char *s)           { sys_syscall1(SYS_KPRINT, (long)s); }
static inline void sys_cls(void)                 { sys_syscall0(SYS_CLS); }
static inline void sys_kprint_dec(uint64 n)      { sys_syscall1(SYS_KPRINT_DEC, (long)n); }
static inline void sys_kprint_hex(long n, char *post) { sys_syscall2(SYS_KPRINT_HEX, n, (long)post); }
static inline void sys_kprint_dec_pad(uint64 n, uint32 w) { sys_syscall2(SYS_KPRINT_DECPAD, (long)n, (long)w); }
static inline void sys_heap_stats(uint32 *u, uint32 *f, uint32 *t) { sys_syscall3(SYS_HEAP_STATS, (long)u, (long)f, (long)t); }
static inline void *sys_kmalloc(size_t sz)       { return (void *)sys_syscall1(SYS_KMALLOC, (long)sz); }
static inline void sys_kfree(void *p)            { sys_syscall1(SYS_KFREE, (long)p); }
static inline int  sys_ide_read(uint32 lba, uint8 cnt, uint8 *buf) { return (int)sys_syscall3(SYS_IDE_READ, lba, cnt, (long)buf); }
static inline int  sys_ide_write(uint32 lba, uint8 *buf) { return (int)sys_syscall2(SYS_IDE_WRITE, lba, (long)buf); }
static inline char *sys_ide_model(void)          { return (char *)sys_syscall0(SYS_IDE_MODEL); }
static inline uint32 sys_ide_sectors(void)       { return (uint32)sys_syscall0(SYS_IDE_SECTORS); }
static inline void sys_fat32_ls(void)            { sys_syscall0(SYS_FAT32_LS); }
static inline int  sys_fat32_cat(const char *n)  { return (int)sys_syscall1(SYS_FAT32_CAT, (long)n); }
static inline void sys_acpi_ls(void)             { sys_syscall0(SYS_ACPI_LS); }
static inline void sys_reboot(void)              { sys_syscall0(SYS_REBOOT); }
static inline long sys_cpu_info(void *buf)       { return sys_syscall1(SYS_CPU_INFO, (long)buf); }
static inline long sys_meminfo(void *buf)        { return sys_syscall1(SYS_MEMINFO, (long)buf); }
static inline int  sys_readchar(void)            { return (int)sys_syscall0(SYS_READCHAR); }
static inline int  sys_wait_input(void)          { return (int)sys_syscall0(SYS_WAIT_INPUT); }
static inline void sys_yield(void)               { sys_syscall0(SYS_YIELD); }
static inline void sys_task_exit(void)           { sys_syscall0(SYS_TASK_EXIT); }
static inline void sys_iso_ls(const char *path)  { sys_syscall1(SYS_ISO_LS, (long)path); }
static inline int  sys_iso_read(const char *path, void *buf, uint32 max) { return (int)sys_syscall3(SYS_ISO_READ, (long)path, (long)buf, (long)max); }
static inline void sys_test_ap(void)                    { sys_syscall0(SYS_TEST_AP); }

// Per-CPU info returned by SYS_CPU_INFO
typedef struct {
    uint8  lapic_id;
    uint8  running;
    uint8  in_usermode;
    uint8  reserved;
    uint32 numa_node;
    uint8  has_numa;
} SysCpuEntry;

#define MAX_SYS_CPUS 16

typedef struct {
    uint32 cpu_count;
    uint32 lapic_base;
    uint32 ioapic_base;
    uint32 numa_node_count;
    SysCpuEntry cpus[MAX_SYS_CPUS];
} SysCpuInfo;

typedef struct {
    uint64 paging_location_end;
    uint64 mem_amount_high;
    uint64 mem_amount_low;
    uint8  mmap_entries;
} SysMemInfo;

#endif
