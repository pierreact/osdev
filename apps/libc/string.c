#include "string.h"

#pragma GCC push_options
#pragma GCC optimize("no-tree-loop-distribute-patterns")

void *memcpy(void *dest, const void *src, size_t n) {
    uint8 *d = (uint8 *)dest;
    const uint8 *s = (const uint8 *)src;
    for (size_t i = 0; i < n; i++)
        d[i] = s[i];
    return dest;
}

void *memset(void *dest, int val, size_t n) {
    uint8 *d = (uint8 *)dest;
    for (size_t i = 0; i < n; i++)
        d[i] = (uint8)val;
    return dest;
}

#pragma GCC pop_options

size_t strlen(const char *s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}
