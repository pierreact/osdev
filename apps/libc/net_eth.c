// Userland trampoline: compile shared eth.c with userland include paths.
// Provide memcpy/memset via string.h instead of kernel/mem.h.
#include "string.h"
#define memcpy __isurus_memcpy_already_included
#include "net/eth.h"
#undef memcpy

// Pull in the shared source (it includes kernel/mem.h which we must skip)
// Instead, redefine the one function directly since it's tiny.
#include "string.h"

const uint8 ETH_BROADCAST[ETH_ADDR_LEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

int eth_build_hdr(uint8 *buf, const uint8 *dst, const uint8 *src, uint16 ethertype) {
    memcpy(buf, dst, ETH_ADDR_LEN);
    memcpy(buf + ETH_ADDR_LEN, src, ETH_ADDR_LEN);
    buf[12] = (uint8)(ethertype >> 8);
    buf[13] = (uint8)(ethertype & 0xFF);
    return ETH_HDR_LEN;
}
