#include <types.h>
#include <system.monitor.h>
#include <system.mem.h>

extern uint8 data_counter_mmap_entries;
extern uint32 MEMMAP_START;
extern uint32 PML4T_LOCATION;
extern uint32 PAGING_LOCATION_END;

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



