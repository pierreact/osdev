#ifndef SYSTEM_PORT_H
#define SYSTEM_PORT_H

#include <types.h>

void   outb(uint16 port, uint8 value);
uint8  inb(uint16 port);
uint16 inw(uint16 port);


#endif

