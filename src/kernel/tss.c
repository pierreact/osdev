#include <types.h>
#include <kernel/tss.h>
#include <kernel/cpu.h>
#include <drivers/monitor.h>

static TSS64 tss[MAX_CPUS];

// GDT64 is defined in kmain.s; TSS descriptor starts at offset 0x30
extern uint8 GDT64[];

static void patch_tss_descriptor(uint64 base, uint16 limit) {
    uint8 *desc = &GDT64[0x30];

    // Bytes 0-1: limit [15:0]
    desc[0] = limit & 0xFF;
    desc[1] = (limit >> 8) & 0xFF;

    // Bytes 2-3: base [15:0]
    desc[2] = base & 0xFF;
    desc[3] = (base >> 8) & 0xFF;

    // Byte 4: base [23:16]
    desc[4] = (base >> 16) & 0xFF;

    // Byte 5: type=9 (available 64-bit TSS), DPL=0, present
    desc[5] = 0x89;

    // Byte 6: limit [19:16] + granularity flags (0)
    desc[6] = (limit >> 16) & 0x0F;

    // Byte 7: base [31:24]
    desc[7] = (base >> 24) & 0xFF;

    // Bytes 8-11: base [63:32]
    desc[8]  = (base >> 32) & 0xFF;
    desc[9]  = (base >> 40) & 0xFF;
    desc[10] = (base >> 48) & 0xFF;
    desc[11] = (base >> 56) & 0xFF;

    // Bytes 12-15: reserved
    desc[12] = 0;
    desc[13] = 0;
    desc[14] = 0;
    desc[15] = 0;
}

void tss_init(void) {
    // Zero all TSS entries
    for (uint32 i = 0; i < MAX_CPUS; i++) {
        uint8 *p = (uint8 *)&tss[i];
        for (uint32 j = 0; j < TSS_SIZE; j++)
            p[j] = 0;
        tss[i].iopb_offset = TSS_SIZE;
    }

    // BSP TSS: set RSP0 to BSP kernel stack
    tss[0].rsp0 = percpu[0].stack_top;

    // Patch GDT64 TSS descriptor with BSP TSS address
    uint64 tss_addr = (uint64)&tss[0];
    patch_tss_descriptor(tss_addr, TSS_SIZE - 1);

    // Load Task Register with TSS selector (0x30)
    __asm__ volatile("ltr %w0" : : "r"((uint16)0x30));

    kprint("TSS: BSP TSS loaded at ");
    kprint_long2hex(tss_addr, "\n");
}

void tss_set_rsp0(uint32 cpu_idx, uint64 rsp0) {
    tss[cpu_idx].rsp0 = rsp0;
}
