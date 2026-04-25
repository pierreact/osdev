#include <net/l2.h>
#include <net/nic.h>
#include <kernel/mem.h>
#include <drivers/monitor.h>
#include <arch/apic.h>

// Pool backing memory for BSP NICs (8 pages = 16 buffers each)
#define BSP_POOL_PAGES 8
static uint8 *mgmt_pool_mem;
static uint8 *inter_pool_mem;

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
// 10.0.2.15 in host byte order. htonl() converts to network byte order
// for comparison with ARP packet fields (which are network byte order).
#define MGMT_IP_DEFAULT    0x0A00020Fu
#define MGMT_MASK_DEFAULT  0xFFFFFF00u       // 255.255.255.0
#define MGMT_GW_DEFAULT    0x0A000202u       // 10.0.2.2
#define MGMT_MTU_DEFAULT   1500

void l2_kern_init(void) {
    uint32 nic_count = nic_get_count();

    NetBackend kern_backend = {
        .send = kern_send,
        .recv = kern_recv,
        .get_mac = kern_get_mac,
        .recv_batch = NULL,     // no batch at NIC layer yet
    };

    // NIC 0: management (has IP, ARP active)
    if (nic_count > 0) {
        mgmt_pool_mem = (uint8 *)alloc_pages(BSP_POOL_PAGES);
        l2_init(&bsp_mgmt, kern_backend, (void *)(uint64)0,
                htonl(MGMT_IP_DEFAULT), BSP_POOL_PAGES, mgmt_pool_mem);
        bsp_mgmt.mask = htonl(MGMT_MASK_DEFAULT);
        bsp_mgmt.gw   = htonl(MGMT_GW_DEFAULT);
        bsp_mgmt.mtu  = MGMT_MTU_DEFAULT;
        bsp_mgmt.forward = 0;
        // Wire INTx for the mgmt NIC so packet arrivals wake the BSP
        // hlt loop without waiting for a timer tick. Best-effort: if
        // the GSI lookup fails (real hardware without _PRT support),
        // we silently fall back to timer-driven wakes.
        nic_enable_intx(0, IRQ_VIRTIO_NET);
        kprint("L2: mgmt NIC 0 IP 10.0.2.15 pool ");
        kprint_dec(pktbuf_pool_total(&bsp_mgmt.pool));
        kprint(" bufs MAC ");
        for (int i = 0; i < 6; i++) {
            if (i > 0) putc(':');
            putc("0123456789abcdef"[(bsp_mgmt.mac[i] >> 4) & 0xF]);
            putc("0123456789abcdef"[bsp_mgmt.mac[i] & 0xF]);
        }
        putc('\n');
    }

    // NIC 1: inter-node DSM (no IP, no ARP replies)
    if (nic_count > 1) {
        inter_pool_mem = (uint8 *)alloc_pages(BSP_POOL_PAGES);
        l2_init(&bsp_inter, kern_backend, (void *)(uint64)1,
                0, BSP_POOL_PAGES, inter_pool_mem);
        kprint("L2: inter NIC 1 pool ");
        kprint_dec(pktbuf_pool_total(&bsp_inter.pool));
        kprint(" bufs (no IP)\n");
    }
}

// Accessors for shell and kernel consumers
L2Context *l2_kern_mgmt(void) {
    return &bsp_mgmt;
}

L2Context *l2_kern_inter(void) {
    return &bsp_inter;
}
