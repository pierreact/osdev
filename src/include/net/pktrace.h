#ifndef ISURUS_NET_PKTRACE_H
#define ISURUS_NET_PKTRACE_H

#include <types.h>

#define PKTRACE_MAX_STAMPS  32
#define PKTRACE_LOG_SIZE    64   // static ring buffer of trace records

// Trace point IDs (grows as layers are added)
enum {
    // L2 layer
    PKT_NIC_RX,             // frame received from virtqueue
    PKT_L2_PARSE,           // Ethernet header parsed
    PKT_ARP_PROCESS,        // ARP handling
    PKT_L2_DELIVER,         // frame delivered to upper layer
    PKT_L2_SEND_ENTER,      // l2_send called
    PKT_L2_SEND_BUILT,      // Ethernet header constructed
    PKT_NIC_TX,             // frame handed to virtqueue
    PKT_NIC_TX_DONE,        // virtqueue TX completion
    // L3 layer
    PKT_IP_PARSE,           // IPv4 header accepted (version, IHL, checksum)
    PKT_IP_DELIVER,         // destination = us, handed to protocol handler
    PKT_IP_FORWARD,         // destination != us, TTL decremented, forwarded
    PKT_ICMP_ECHO_RX,       // ICMP Echo Request received
    PKT_ICMP_ECHO_TX,       // ICMP Echo Reply sent
    // Services
    PKT_NET_SERVICE_TICK,   // BSP net_service drained one or more frames
    // Future: PKT_UDP_PARSE, PKT_TCP_PARSE, PKT_APP_RECV, PKT_APP_SEND
    PKT_POINT_COUNT
};

typedef struct {
    uint64 tsc;              // RDTSC timestamp
    uint16 point;            // trace point ID (enum above)
    uint16 buf_capacity;     // relevant buffer total size
    uint16 buf_used;         // relevant buffer current occupancy
    uint16 payload_len;      // payload size at this processing stage
} PkTraceStamp;

typedef struct {
    uint64       tag;        // hex ID chosen by caller (e.g. 0xABCD0001)
    uint32       seq;        // auto-incremented per frame within this tag
    uint8        stamp_count;
    uint8        active;     // 1 between begin/end
    uint8        reserved[2];
    PkTraceStamp stamps[PKTRACE_MAX_STAMPS];
} PkTrace;

// TSC read
static inline uint64 rdtsc(void) {
    uint32 lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64)hi << 32) | lo;
}

// Stamp macro: zero cost when trace is NULL
#define PKT_STAMP(trace, pt, bcap, bused, plen) do { \
    if ((trace) && (trace)->active && \
        (trace)->stamp_count < PKTRACE_MAX_STAMPS) { \
        uint8 _i = (trace)->stamp_count; \
        (trace)->stamps[_i].tsc = rdtsc(); \
        (trace)->stamps[_i].point = (pt); \
        (trace)->stamps[_i].buf_capacity = (bcap); \
        (trace)->stamps[_i].buf_used = (bused); \
        (trace)->stamps[_i].payload_len = (plen); \
        (trace)->stamp_count++; \
    } \
} while(0)

// Lifecycle
PkTrace *pktrace_begin(uint64 tag);   // grab from static ring, set active=1
void     pktrace_next(PkTrace *t);    // increment seq, reset stamps for next frame
void     pktrace_end(PkTrace *t);     // set active=0, record persists in log

// Output
void     pktrace_dump(PkTrace *t);    // print stamps to console
void     pktrace_dump_all(void);      // dump entire log ring

// Access log ring directly. Kernel-only (defined in pktrace_kern.c);
// shell uses it for sys.net.trace. Apps do not link this symbol.
PkTrace *pktrace_get_log(uint32 *count_out, uint32 *head_out);

// Output adapters. Each build environment provides its own
// implementation so the shared pktrace.c can call them uniformly:
//   kernel: src/net/pktrace_kern.c wraps kprint / kprint_dec /
//           kprint_long2hex.
//   libc:   apps/libc/pktrace_adapter.c wraps puts / print_dec /
//           print_hex8.
void pktrace_put_str(const char *s);
void pktrace_put_dec(uint64 n);
void pktrace_put_hex(uint64 n);
void pktrace_put_char(char c);

// Trace point name for display
const char *pktrace_point_name(uint16 point);

#endif
