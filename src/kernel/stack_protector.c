#include <types.h>
#include <drivers/monitor.h>

/*
 * Kernel-provided stack protector runtime for freestanding builds.
 * GCC reads the canary from this global (-mstack-protector-guard=global).
 */
uint64 __stack_chk_guard = 0xD4E4C0DEC0FFEE11ULL;

__attribute__((noreturn))
void __stack_chk_fail(void) {
    char msg[] = "\n[FATAL] Stack smashing detected. System halted.\n";
    kprint(msg);

    for (;;) {
        __asm__ __volatile__("cli; hlt");
    }
}

__attribute__((noreturn))
void __stack_chk_fail_local(void) {
    __stack_chk_fail();
}
