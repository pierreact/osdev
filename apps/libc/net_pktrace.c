// Userland pktrace implementation (mirrors src/net/pktrace.c with userland includes)
#include "net/pktrace.h"
#include "stdio.h"

static PkTrace trace_log[PKTRACE_LOG_SIZE];
static uint32 trace_log_head;

static const char *point_names[] = {
    [PKT_NIC_RX]        = "NIC_RX",
    [PKT_L2_PARSE]      = "L2_PARSE",
    [PKT_ARP_PROCESS]   = "ARP_PROC",
    [PKT_L2_DELIVER]    = "L2_DELIVER",
    [PKT_L2_SEND_ENTER] = "L2_SEND",
    [PKT_L2_SEND_BUILT] = "L2_BUILT",
    [PKT_NIC_TX]        = "NIC_TX",
    [PKT_NIC_TX_DONE]   = "NIC_TX_DONE",
};

const char *pktrace_point_name(uint16 point) {
    if (point < PKT_POINT_COUNT && point_names[point])
        return point_names[point];
    return "?";
}

PkTrace *pktrace_begin(uint64 tag) {
    uint32 idx = trace_log_head % PKTRACE_LOG_SIZE;
    trace_log_head++;
    PkTrace *t = &trace_log[idx];
    t->tag = tag;
    t->seq = 0;
    t->stamp_count = 0;
    t->active = 1;
    return t;
}

void pktrace_next(PkTrace *t) {
    if (!t) return;
    t->seq++;
    t->stamp_count = 0;
}

void pktrace_end(PkTrace *t) {
    if (!t) return;
    t->active = 0;
}

void pktrace_dump(PkTrace *t) {
    if (!t) return;
    puts("TRACE tag=");
    print_dec(t->tag);
    puts(" seq=");
    print_dec(t->seq);
    puts(" stamps=");
    print_dec(t->stamp_count);
    putc('\n');
    uint64 prev_tsc = 0;
    for (uint8 i = 0; i < t->stamp_count; i++) {
        PkTraceStamp *s = &t->stamps[i];
        uint64 delta = (prev_tsc > 0) ? (s->tsc - prev_tsc) : 0;
        puts("  ");
        puts((char *)pktrace_point_name(s->point));
        puts("  +");
        print_dec(delta);
        puts(" cyc  buf ");
        print_dec(s->buf_used);
        putc('/');
        print_dec(s->buf_capacity);
        puts("  len ");
        print_dec(s->payload_len);
        putc('\n');
        prev_tsc = s->tsc;
    }
}

void pktrace_dump_all(void) {
    uint32 count = trace_log_head;
    if (count > PKTRACE_LOG_SIZE) count = PKTRACE_LOG_SIZE;
    if (count == 0) {
        puts("TRACE: no records\n");
        return;
    }
    puts("TRACE: ");
    print_dec(count);
    puts(" record(s)\n");
    uint32 start = 0;
    if (trace_log_head > PKTRACE_LOG_SIZE)
        start = trace_log_head - PKTRACE_LOG_SIZE;
    for (uint32 i = start; i < trace_log_head; i++) {
        PkTrace *t = &trace_log[i % PKTRACE_LOG_SIZE];
        if (t->stamp_count > 0)
            pktrace_dump(t);
    }
}
