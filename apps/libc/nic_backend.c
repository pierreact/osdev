#include "nic_backend.h"
#include "syscall.h"
#include "string.h"

// ctx is the ThreadMeta pointer; we only need it for get_mac.
// The kernel resolves the NIC from the calling CPU automatically.
static int nb_send(void *ctx, const uint8 *frame, uint32 len) {
    (void)ctx;
    return (int)syscall2(SYS_NIC_SEND, (long)frame, (long)len);
}

static int nb_recv(void *ctx, uint8 *buf, uint32 *len) {
    (void)ctx;
    return (int)syscall2(SYS_NIC_RECV, (long)buf, (long)len);
}

static void nb_get_mac(void *ctx, uint8 *mac_out) {
    const ThreadMeta *m = (const ThreadMeta *)ctx;
    memcpy(mac_out, m->nic_mac, 6);
}

NetBackend nic_backend_make(const ThreadMeta *meta) {
    (void)meta;
    NetBackend be = {
        .send       = nb_send,
        .recv       = nb_recv,
        .get_mac    = nb_get_mac,
        .recv_batch = 0,
    };
    return be;
}
