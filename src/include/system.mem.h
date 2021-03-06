#ifndef MEM_H
#define MEM_H
#include <types.h>

void *kmalloc(size_t size);
void kfree(void* ptr);

void inc_256(uint8* slot, uint8* write);

#define E820_TYPE_FREE              1
#define E820_TYPE_NONFREE           2
#define E820_TYPE_ACPI_RECLAIMABLE  3
#define E820_TYPE_ACPI_NVS          4
#define E820_TYPE_BAD_MEMORY        5

#define MEMPGING_LAST_PML4E 0b1
#define MEMPGING_LAST_PDPE  0b10
#define MEMPGING_LAST_PDE   0b100
#define MEMPGING_LAST_PTE   0b1000

typedef struct _E820E {
    uint64 base     __attribute__((packed));
    uint64 length   __attribute__((packed));

    uint32 type     __attribute__((packed));
    uint32 acpiattr __attribute__((packed));
} E820E;



typedef struct _MEM_PAGING_PML4E {
    uint8   P           :  1 __attribute__((packed)); // 1  bit     0
    uint8   RW          :  1 __attribute__((packed)); // 1  bit     1
    uint8   US          :  1 __attribute__((packed)); // 1  bit     2
    uint8   PWT         :  1 __attribute__((packed)); // 1  bit     3
    uint8   PCD         :  1 __attribute__((packed)); // 1  bit     4
    uint8   A           :  1 __attribute__((packed)); // 1  bit     5
    uint8   MBZIGN      :  3 __attribute__((packed)); // 3  bits    6-8
    uint8   AVL         :  3 __attribute__((packed)); // 3  bits    9-11
    uint64  baddr       : 40 __attribute__((packed)); // 40 bits    12-51
    uint16  AVL2        : 11 __attribute__((packed)); // 11 bits    52-62
    uint8   NX          :  1 __attribute__((packed)); // 1  bit     63
} MEM_PAGING_PML4E;


typedef MEM_PAGING_PML4E MEM_PAGING_PDPE; // Same
typedef MEM_PAGING_PML4E MEM_PAGING_PDE;  // Same

typedef struct _MEM_PAGING_PTE {
    uint8   P           :  1 __attribute__((packed)); // 1  bit     0
    uint8   RW          :  1 __attribute__((packed)); // 1  bit     1
    uint8   US          :  1 __attribute__((packed)); // 1  bit     2
    uint8   PWT         :  1 __attribute__((packed)); // 1  bit     3
    uint8   PCD         :  1 __attribute__((packed)); // 1  bit     4
    uint8   A           :  1 __attribute__((packed)); // 1  bit     5
    uint8   D           :  1 __attribute__((packed)); // 1  bit     6
    uint8   PAT         :  1 __attribute__((packed)); // 1  bit     7
    uint8   G           :  1 __attribute__((packed)); // 1  bit     8
    uint8   AVL         :  3 __attribute__((packed)); // 3  bits    9-11
    uint64  baddr       : 40 __attribute__((packed)); // 40 bits    12-51
    uint16  AVL2        : 11 __attribute__((packed)); // 11 bits    52-62
    uint8   NX          :  1 __attribute__((packed)); // 1  bit     63
} MEM_PAGING_PTE;

#endif // MEM_H






