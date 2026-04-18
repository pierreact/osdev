#include <net/eth.h>
#include <kernel/mem.h>

const uint8 ETH_BROADCAST[ETH_ADDR_LEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

int eth_build_hdr(uint8 *buf, const uint8 *dst, const uint8 *src, uint16 ethertype) {
    memcpy(buf, dst, ETH_ADDR_LEN);
    memcpy(buf + ETH_ADDR_LEN, src, ETH_ADDR_LEN);
    // Store ethertype in network byte order
    buf[12] = (uint8)(ethertype >> 8);
    buf[13] = (uint8)(ethertype & 0xFF);
    return ETH_HDR_LEN;
}
