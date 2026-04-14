#include <types.h>
#include <drivers/monitor.h>
#include <kernel/mem.h>

extern uint8 data_counter_mmap_entries;
extern uint32 MEMMAP_START;
extern uint32 PML4T_LOCATION;
extern uint32 PAGING_LOCATION_END;
extern uint8 kernelEnd[];

// Page table entry flags
#define PTE_PRESENT   0x01
#define PTE_WRITABLE  0x02
#define PTE_USER      0x04
#define PTE_PWT       0x08
#define PTE_PCD       0x10

// Heap: dynamic range from end of BSS to 0x9F000
#define BLOCK_SIZE      4096
#define HEAP_CEILING    0x9F000
#define MAX_BITMAP_SIZE 32              // supports up to 256 blocks (~1MB)

static uint64 heap_start;
static uint64 heap_end;
static uint32 num_blocks;
static uint8 heap_bitmap[MAX_BITMAP_SIZE];

void init_memmgr() {
    kprint("Initializing memory manager.\n");
    kprint_long2hex(data_counter_mmap_entries,  "MMAP Entries\n");
    kprint_long2hex((long) &MEMMAP_START,       "MEMMAP_START\n");
    kprint_long2hex(PML4T_LOCATION,             "PML4T_LOCATION\n");
    kprint_long2hex(PAGING_LOCATION_END,        "PAGING_LOCATION_END\n");
}

// Allocate n contiguous pages from above PAGING_LOCATION_END (high memory)
uint64 alloc_pages(uint32 n) {
    uint64 addr = (uint64)PAGING_LOCATION_END;
    PAGING_LOCATION_END += n * 4096;
    return addr;
}

// Allocate 64KB BSP stack from high memory, return stack top
uint64 alloc_bsp_stack(void) {
    uint64 base = alloc_pages(16);      // 16 pages = 64KB
    kprint("BSP stack: ");
    kprint_long2hex(base, " - ");
    kprint_long2hex(base + 0x10000, "\n");
    return base + 0x10000;              // stack grows down from top
}

void heap_init() {
    // Heap starts at page-aligned address after all BSS
    heap_start = ((uint64)kernelEnd + 4095) & ~4095ULL;
    heap_end = HEAP_CEILING;
    num_blocks = (heap_end - heap_start) / BLOCK_SIZE;

    // Clamp to bitmap capacity
    if (num_blocks > MAX_BITMAP_SIZE * 8)
        num_blocks = MAX_BITMAP_SIZE * 8;

    for(uint32 i = 0; i < MAX_BITMAP_SIZE; i++) {
        heap_bitmap[i] = 0;
    }

    kprint("Heap: ");
    kprint_long2hex(heap_start, " - ");
    kprint_long2hex(heap_end, " (");
    kprint_dec(num_blocks);
    kprint(" blocks)\n");
}

void *kmalloc(size_t size) {
    if(size == 0) return NULL;

    for(uint32 i = 0; i < num_blocks; i++) {
        uint32 byte_idx = i / 8;
        uint32 bit_idx = i % 8;

        if(!(heap_bitmap[byte_idx] & (1 << bit_idx))) {
            heap_bitmap[byte_idx] |= (1 << bit_idx);
            return (void*)(uint64)(heap_start + i * BLOCK_SIZE);
        }
    }

    return NULL;
}

void kfree(void *ptr) {
    if(ptr == NULL) return;

    uint64 addr = (uint64)ptr;
    if(addr < heap_start || addr >= heap_end) return;

    uint32 block = (addr - heap_start) / BLOCK_SIZE;
    uint32 byte_idx = block / 8;
    uint32 bit_idx = block % 8;

    heap_bitmap[byte_idx] &= ~(1 << bit_idx);
}

// Count used/free blocks by scanning the bitmap
void heap_stats(uint32 *used_blocks, uint32 *free_blocks, uint32 *total_blocks) {
    uint32 used = 0;
    for(uint32 i = 0; i < num_blocks; i++) {
        uint32 byte_idx = i / 8;
        uint32 bit_idx = i % 8;
        if(heap_bitmap[byte_idx] & (1 << bit_idx))
            used++;
    }
    *used_blocks = used;
    *free_blocks = num_blocks - used;
    *total_blocks = num_blocks;
}

// Allocate a new page-table page from above PAGING_LOCATION_END
static uint64 alloc_page_table(void) {
    uint64 addr = alloc_pages(1);
    // Zero all 512 entries (4KB page)
    volatile uint64 *p = (volatile uint64 *)addr;
    for (int i = 0; i < 512; i++) p[i] = 0;
    return addr;
}

// Identity-map [phys_start, phys_start+size) as uncacheable MMIO
void map_mmio_range(uint64 phys_start, uint64 size) {
    uint64 end = phys_start + size;
    uint64 pml4t_addr = (uint64)PML4T_LOCATION;

    for (uint64 vaddr = phys_start; vaddr < end; vaddr += 4096) {
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
        volatile uint64 *pdpt = (volatile uint64 *)(pml4[pml4_idx] & 0x000FFFFFFFFFF000ULL);

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
}

// Map APIC MMIO region (legacy wrapper)
void map_mmio_region(void) {
    map_mmio_range(0xFEC00000ULL, 0x300000ULL);
    kprint("MEM: MMIO region 0xFEC00000-0xFEF00000 mapped\n");
}

// Prevent GCC from replacing these loops with calls to themselves
#pragma GCC push_options
#pragma GCC optimize("no-tree-loop-distribute-patterns")

void *memcpy(void *dest, const void *src, size_t n) {
    uint8 *d = (uint8 *)dest;
    const uint8 *s = (const uint8 *)src;
    for (size_t i = 0; i < n; i++)
        d[i] = s[i];
    return dest;
}

void *memset(void *dest, int val, size_t n) {
    uint8 *d = (uint8 *)dest;
    for (size_t i = 0; i < n; i++)
        d[i] = (uint8)val;
    return dest;
}

#pragma GCC pop_options
