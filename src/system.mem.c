#include <types.h>
#include <system.monitor.h>

void init_memmgr();

char *cfunc_called = "Initializing memory manager";

void init_memmgr() {
    kprint(cfunc_called);
}


