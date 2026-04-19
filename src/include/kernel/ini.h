#ifndef KERNEL_INI_H
#define KERNEL_INI_H

#include <types.h>

// Minimal INI parser. Supports [section], key=value, # comments.
// Calls handler for each key=value pair found.
// Returns 0 on success, -1 on parse error.
typedef void (*ini_handler)(const char *section, const char *key,
                            const char *value, void *user);

int ini_parse(const char *buf, uint32 len, ini_handler handler, void *user);

#endif
