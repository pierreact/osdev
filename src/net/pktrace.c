// Shared packet-trace core. Compiled into both the kernel build
// (src/Makefile) and the libc build (apps/libc/Makefile). All console
// I/O goes through the four pktrace_put_* adapter functions; kernel
// and libc each provide their own implementations.
//
// pktrace_get_log() lives in the kernel-only src/net/pktrace_kern.c
// so apps don't pull it in.

#include <net/pktrace.h>

// Static ring buffer of trace records. Visible to pktrace_kern.c as
// an extern so the kernel-only pktrace_get_log() can hand it out.
PkTrace pktrace_log_ring[PKTRACE_LOG_SIZE];
uint32  pktrace_log_head;   // next slot to allocate

static const char *point_names[] = {
    [PKT_NIC_RX]        = "NIC_RX",
    [PKT_L2_PARSE]      = "L2_PARSE",
    [PKT_ARP_PROCESS]   = "ARP_PROC",
    [PKT_L2_DELIVER]    = "L2_DELIVER",
    [PKT_L2_SEND_ENTER] = "L2_SEND",
    [PKT_L2_SEND_BUILT] = "L2_BUILT",
    [PKT_NIC_TX]        = "NIC_TX",
    [PKT_NIC_TX_DONE]   = "NIC_TX_DONE",
    [PKT_IP_PARSE]      = "IP_PARSE",
    [PKT_IP_DELIVER]    = "IP_DELIVER",
    [PKT_IP_FORWARD]    = "IP_FORWARD",
    [PKT_ICMP_ECHO_RX]  = "ICMP_ECHO_RX",
    [PKT_ICMP_ECHO_TX]  = "ICMP_ECHO_TX",
};

const char *pktrace_point_name(uint16 point) {
    if (point < PKT_POINT_COUNT && point_names[point])
        return point_names[point];
    return "?";
}

PkTrace *pktrace_begin(uint64 tag) {
    uint32 idx = pktrace_log_head % PKTRACE_LOG_SIZE;
    pktrace_log_head++;

    PkTrace *t = &pktrace_log_ring[idx];
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

    pktrace_put_str("TRACE tag=");
    pktrace_put_hex(t->tag);
    pktrace_put_str(" seq=");
    pktrace_put_dec(t->seq);
    pktrace_put_str(" stamps=");
    pktrace_put_dec(t->stamp_count);
    pktrace_put_char('\n');

    uint64 prev_tsc = 0;
    for (uint8 i = 0; i < t->stamp_count; i++) {
        PkTraceStamp *s = &t->stamps[i];
        uint64 delta = (prev_tsc > 0) ? (s->tsc - prev_tsc) : 0;

        pktrace_put_str("  ");
        pktrace_put_str((char *)pktrace_point_name(s->point));
        pktrace_put_str("  +");
        pktrace_put_dec(delta);
        pktrace_put_str(" cyc  buf ");
        pktrace_put_dec(s->buf_used);
        pktrace_put_char('/');
        pktrace_put_dec(s->buf_capacity);
        pktrace_put_str("  len ");
        pktrace_put_dec(s->payload_len);
        pktrace_put_char('\n');

        prev_tsc = s->tsc;
    }
}

void pktrace_dump_all(void) {
    uint32 count = pktrace_log_head;
    if (count > PKTRACE_LOG_SIZE)
        count = PKTRACE_LOG_SIZE;

    if (count == 0) {
        pktrace_put_str("TRACE: no records\n");
        return;
    }

    pktrace_put_str("TRACE: ");
    pktrace_put_dec(count);
    pktrace_put_str(" record(s)\n");

    // Dump from oldest to newest
    uint32 start = 0;
    if (pktrace_log_head > PKTRACE_LOG_SIZE)
        start = pktrace_log_head - PKTRACE_LOG_SIZE;

    for (uint32 i = start; i < pktrace_log_head; i++) {
        PkTrace *t = &pktrace_log_ring[i % PKTRACE_LOG_SIZE];
        if (t->stamp_count > 0)
            pktrace_dump(t);
    }
}
