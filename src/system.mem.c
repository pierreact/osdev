#include <types.h>
#include <system.monitor.h>
#include <system.mem.h>

// MEMMAP
extern uint8 data_counter_mmap_entries;
extern uint32 MEMMAP_START;
// /MEMMAP

// Paging structures start
extern uint32 PML4T_LOCATION;
extern uint32 PAGING_LOCATION_END;
// Paging structures end

void init_memmgr();
char *cfunc_called = "Initializing memory manager.\n";
//static volatile uint16 *meminfo_start = (uint16 *)0xC0000;


typedef struct _PAGE_INFO {
    uint8   info;
} PAGE_INFO;

typedef struct _MEMINFO {
    PAGE_INFO *pi;
} MEMINFO;





// FFFFFFUUUFFUFFFFFFFUFFFFUFUFUUUFUFUUUFFUFUFUFUFUUUUUUUUUFFFFFFFF
// FFFFFF   FF FFFFFFF FFFF F F   F F   FF F F F F         FFFFFFFF
//       UUU  U       U    U U UUU U UUU  U U U U UUUUUUUUU        


void init_memmgr() {
    kprint(cfunc_called);
    kprint_long2hex(data_counter_mmap_entries,  "MMAP Entries\n");
    kprint_long2hex((long) &MEMMAP_START,       "MEMMAP_START\n");
    kprint_long2hex(PML4T_LOCATION,             "PML4T_LOCATION\n");
    kprint_long2hex(PAGING_LOCATION_END,        "PAGING_LOCATION_END\n");

}


void *kmalloc(size_t size) {
    // How many concurrent blocks of memory do I need for that?
    // Look at the memory map
    // Mark blocks as used
    // return pointer
    return NULL;
}

void kfree(void *ptr) {
//    if(ptr == NULL) { return; }
    // Mark blocks as free

}



