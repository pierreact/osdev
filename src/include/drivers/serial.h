#ifndef SYSTEM_SERIAL_H
#define SYSTEM_SERIAL_H

#include <types.h>

void serial_init(void);
void serial_putc(char c);
void serial_print(char *s);
void serial_driver(void);

#endif
