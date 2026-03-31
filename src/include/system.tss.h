#ifndef SYSTEM_TSS_H
#define SYSTEM_TSS_H

#include <types.h>

typedef struct __attribute__((packed)) {
    uint32 reserved0;
    uint64 rsp0;
    uint64 rsp1;
    uint64 rsp2;
    uint64 reserved1;
    uint64 ist1;
    uint64 ist2;
    uint64 ist3;
    uint64 ist4;
    uint64 ist5;
    uint64 ist6;
    uint64 ist7;
    uint64 reserved2;
    uint16 reserved3;
    uint16 iopb_offset;
} TSS64;

#define TSS_SIZE 104

void tss_init(void);
void tss_set_rsp0(uint32 cpu_idx, uint64 rsp0);

#endif
