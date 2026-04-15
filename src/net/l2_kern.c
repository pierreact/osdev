#include <net/l2.h>
#include <net/nic.h>
#include <drivers/monitor.h>

// BSP L2 contexts (management + inter-node)
static L2Context bsp_mgmt;
static L2Context bsp_inter;

// Kernel backend: wraps nic_send/nic_recv using NIC slot index
static int kern_send(void *ctx, const uint8 *frame, uint32 len) {
    uint32 idx = (uint32)(uint64)ctx;
    return nic_send(idx, frame, len);
}

static int kern_recv(void *ctx, uint8 *buf, uint32 *len) {
    uint32 idx = (uint32)(uint64)ctx;
    return nic_recv(idx, buf, len);
}

static void kern_get_mac(void *ctx, uint8 *mac) {
    uint32 idx = (uint32)(uint64)ctx;
    nic_get_mac(idx, mac);
}

// Default management IP for QEMU user-mode networking (host byte order)
#define MGMT_IP_DEFAULT  0x0A00020Fu    // 10.0.2.15

void l2_kern_init(void) {
    uint32 nic_count = nic_get_count();

    NetBackend kern_backend = {
        .send = kern_send,
        .recv = kern_recv,
        .get_mac = kern_get_mac,
    };

    // NIC 0: management (has IP, ARP active)
    if (nic_count > 0) {
        l2_init(&bsp_mgmt, kern_backend, (void *)(uint64)0, htonl(MGMT_IP_DEFAULT));
        kprint("L2: mgmt NIC 0 IP 10.0.2.15 MAC ");
        for (int i = 0; i < 6; i++) {
            if (i > 0) putc(':');
            putc("0123456789abcdef"[(bsp_mgmt.mac[i] >> 4) & 0xF]);
            putc("0123456789abcdef"[bsp_mgmt.mac[i] & 0xF]);
        }
        putc('\n');
    }

    // NIC 1: inter-node DSM (no IP, no ARP replies)
    if (nic_count > 1) {
        l2_init(&bsp_inter, kern_backend, (void *)(uint64)1, 0);
        kprint("L2: inter NIC 1 (no IP)\n");
    }
}

// Accessors for shell and kernel consumers
L2Context *l2_kern_mgmt(void) {
    return &bsp_mgmt;
}

L2Context *l2_kern_inter(void) {
    return &bsp_inter;
}
