#include <types.h>
#include <system.monitor.h>
#include <system.mem.h>

extern uint8 data_counter_mmap_entries;
extern uint32 MEMMAP_START;
extern uint32 PML4T_LOCATION;
extern uint32 PAGING_LOCATION_END;

// Page table entry flags
#define PTE_PRESENT   0x01
#define PTE_WRITABLE  0x02
#define PTE_USER      0x04
#define PTE_PWT       0x08
#define PTE_PCD       0x10

// Heap: 0xD000-0x9F000 (~584 KB)
#define HEAP_START      0xD000
#define HEAP_END        0x9F000
#define BLOCK_SIZE      4096
#define NUM_BLOCKS      ((HEAP_END - HEAP_START) / BLOCK_SIZE)
#define BITMAP_SIZE     ((NUM_BLOCKS + 7) / 8)

static uint8 heap_bitmap[BITMAP_SIZE];

void init_memmgr() {
    kprint("Initializing memory manager.\n");
    kprint_long2hex(data_counter_mmap_entries,  "MMAP Entries\n");
    kprint_long2hex((long) &MEMMAP_START,       "MEMMAP_START\n");
    kprint_long2hex(PML4T_LOCATION,             "PML4T_LOCATION\n");
    kprint_long2hex(PAGING_LOCATION_END,        "PAGING_LOCATION_END\n");
}

void heap_init() {
    for(int i = 0; i < BITMAP_SIZE; i++) {
        heap_bitmap[i] = 0;
    }
    kprint("Heap initialized: ");
    kprint_dec(NUM_BLOCKS);
    kprint(" blocks\n");
}

void *kmalloc(size_t size) {
    if(size == 0) return NULL;
    
    for(uint32 i = 0; i < NUM_BLOCKS; i++) {
        uint32 byte_idx = i / 8;
        uint32 bit_idx = i % 8;
        
        if(!(heap_bitmap[byte_idx] & (1 << bit_idx))) {
            heap_bitmap[byte_idx] |= (1 << bit_idx);
            return (void*)(uint64)(HEAP_START + i * BLOCK_SIZE);
        }
    }
    
    return NULL;
}

void kfree(void *ptr) {
    if(ptr == NULL) return;

    uint64 addr = (uint64)ptr;
    if(addr < HEAP_START || addr >= HEAP_END) return;

    uint32 block = (addr - HEAP_START) / BLOCK_SIZE;
    uint32 byte_idx = block / 8;
    uint32 bit_idx = block % 8;

    heap_bitmap[byte_idx] &= ~(1 << bit_idx);
}

// Count used/free blocks by scanning the bitmap
void heap_stats(uint32 *used_blocks, uint32 *free_blocks, uint32 *total_blocks) {
    uint32 used = 0;
    for(uint32 i = 0; i < NUM_BLOCKS; i++) {
        uint32 byte_idx = i / 8;
        uint32 bit_idx = i % 8;
        if(heap_bitmap[byte_idx] & (1 << bit_idx))
            used++;
    }
    *used_blocks = used;
    *free_blocks = NUM_BLOCKS - used;
    *total_blocks = NUM_BLOCKS;
}

// Allocate a new page-table page from above PAGING_LOCATION_END
static uint64 alloc_page_table(void) {
    uint64 addr = (uint64)PAGING_LOCATION_END;
    // Zero all 512 entries (4KB page)
    volatile uint64 *p = (volatile uint64 *)addr;
    for (int i = 0; i < 512; i++) p[i] = 0;
    // Bump the allocator forward
    PAGING_LOCATION_END += 4096;
    return addr;
}

// Map APIC MMIO region (0xFEC00000 - 0xFEF00000) into page tables
// These are above the identity-mapped range. We walk the existing
// PML4T and add entries as needed.
void map_mmio_region(void) {
    uint64 start = 0xFEC00000ULL;
    uint64 end   = 0xFEF00000ULL;

    uint64 pml4t_addr = (uint64)PML4T_LOCATION;

    for (uint64 vaddr = start; vaddr < end; vaddr += 4096) {
        uint32 pml4_idx = (vaddr >> 39) & 0x1FF;
        uint32 pdpt_idx = (vaddr >> 30) & 0x1FF;
        uint32 pd_idx   = (vaddr >> 21) & 0x1FF;
        uint32 pt_idx   = (vaddr >> 12) & 0x1FF;

        volatile uint64 *pml4 = (volatile uint64 *)pml4t_addr;

        // Check/create PDPT entry
        if (!(pml4[pml4_idx] & PTE_PRESENT)) {
            uint64 new_pdpt = alloc_page_table();
            pml4[pml4_idx] = new_pdpt | PTE_PRESENT | PTE_WRITABLE | PTE_USER | PTE_PWT;
        }
        volatile uint64 *pdpt = (volatile uint64 *)(pml4[pml4_idx] & 0x000FFFFFFFFFF000ULL); // Strip flags, keep physical address

        // Check/create PD entry
        if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
            uint64 new_pd = alloc_page_table();
            pdpt[pdpt_idx] = new_pd | PTE_PRESENT | PTE_WRITABLE | PTE_USER | PTE_PWT;
        }
        volatile uint64 *pd = (volatile uint64 *)(pdpt[pdpt_idx] & 0x000FFFFFFFFFF000ULL);

        // Check/create PT entry
        if (!(pd[pd_idx] & PTE_PRESENT)) {
            uint64 new_pt = alloc_page_table();
            pd[pd_idx] = new_pt | PTE_PRESENT | PTE_WRITABLE | PTE_USER | PTE_PWT;
        }
        volatile uint64 *pt = (volatile uint64 *)(pd[pd_idx] & 0x000FFFFFFFFFF000ULL);

        // Map the page: identity map with cache disable (PCD) for MMIO
        if (!(pt[pt_idx] & PTE_PRESENT)) {
            pt[pt_idx] = vaddr | PTE_PRESENT | PTE_WRITABLE | PTE_PCD | PTE_PWT;
        }
    }

    // Flush TLB by reloading CR3
    uint64 cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile("mov %0, %%cr3" : : "r"(cr3));

    kprint("MEM: MMIO region 0xFEC00000-0xFEF00000 mapped\n");
}

