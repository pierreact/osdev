// Kernel-only pktrace bits: the output adapter implementations that
// wrap <drivers/monitor.h> console I/O, plus the log-ring accessor
// used by the shell (sys.net.trace). Apps do NOT link this file;
// they get the same adapter surface from apps/libc/pktrace_adapter.c.

#include <net/pktrace.h>
#include <drivers/monitor.h>

// Log ring is defined in the shared pktrace.c as a non-static global.
extern PkTrace pktrace_log_ring[PKTRACE_LOG_SIZE];
extern uint32  pktrace_log_head;

void pktrace_put_str(const char *s) {
    kprint((char *)s);
}

void pktrace_put_dec(uint64 n) {
    kprint_dec(n);
}

void pktrace_put_hex(uint64 n) {
    kprint_long2hex((long)n, "");
}

void pktrace_put_char(char c) {
    putc(c);
}

PkTrace *pktrace_get_log(uint32 *count_out, uint32 *head_out) {
    uint32 count = pktrace_log_head;
    if (count > PKTRACE_LOG_SIZE) count = PKTRACE_LOG_SIZE;
    *count_out = count;
    *head_out = pktrace_log_head;
    return pktrace_log_ring;
}
