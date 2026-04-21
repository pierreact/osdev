// String and argument-parsing utilities used across shell command handlers.

#include "shell_internal.h"

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n > 0 && *s1 && (*s1 == *s2)) { s1++; s2++; n--; }
    if (n == 0) return 0;
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

size_t strlen(const char *s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

int starts_with(const char *str, const char *prefix) {
    while (*prefix) {
        if (*str != *prefix) return 0;
        str++; prefix++;
    }
    return 1;
}

uint32 parse_number(const char *str) {
    uint32 result = 0;
    if (str[0] == '0' && str[1] == 'x') {
        str += 2;
        while (*str) {
            char c = *str;
            if (c >= '0' && c <= '9') result = result * 16 + (c - '0');
            else if (c >= 'a' && c <= 'f') result = result * 16 + (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') result = result * 16 + (c - 'A' + 10);
            else break;
            str++;
        }
    } else {
        while (*str >= '0' && *str <= '9') {
            result = result * 10 + (*str - '0');
            str++;
        }
    }
    return result;
}

// Parse a dotted-decimal IPv4 address string into network byte order.
// Returns 0 on failure.
uint32 parse_ipv4(const char *s) {
    uint32 octets[4] = {0, 0, 0, 0};
    int idx = 0;
    while (*s && idx < 4) {
        if (*s >= '0' && *s <= '9') {
            octets[idx] = octets[idx] * 10 + (*s - '0');
        } else if (*s == '.') {
            idx++;
        } else {
            return 0;
        }
        s++;
    }
    if (idx != 3) return 0;
    for (int i = 0; i < 4; i++)
        if (octets[i] > 255) return 0;
    // Network byte order (big-endian)
    return (octets[0] << 24) | (octets[1] << 16) | (octets[2] << 8) | octets[3];
}
