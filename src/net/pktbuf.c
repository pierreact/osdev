#include <net/pktbuf.h>
#include <memops.h>

int pktbuf_pool_init(PktBufPool *pool, uint32 n_pages, uint8 *backing_memory) {
    memset(pool, 0, sizeof(PktBufPool));

    pool->pages = backing_memory;
    pool->page_count = n_pages;
    pool->total_slots = n_pages * PKTBUF_SLOTS_PER_PAGE;
    pool->free_count = pool->total_slots;
    pool->free_list = NULL;

    // Chain all slots into the free list
    for (uint32 i = 0; i < pool->total_slots; i++) {
        PktBuf *buf = (PktBuf *)(pool->pages + (uint64)i * PKTBUF_SLOT_SIZE);
        buf->data_off = PKTBUF_HDR_SIZE;
        buf->data_len = 0;
        buf->pool_idx = 0;
        buf->flags = 0;
        buf->next = pool->free_list;
        pool->free_list = buf;
    }

    return 0;
}

PktBuf *pktbuf_alloc(PktBufPool *pool) {
    if (!pool->free_list)
        return NULL;

    PktBuf *buf = pool->free_list;
    pool->free_list = buf->next;
    pool->free_count--;

    buf->next = NULL;
    buf->data_off = PKTBUF_HDR_SIZE;
    buf->data_len = 0;
    return buf;
}

void pktbuf_free(PktBufPool *pool, PktBuf *buf) {
    if (!buf) return;
    buf->next = pool->free_list;
    pool->free_list = buf;
    pool->free_count++;
}
