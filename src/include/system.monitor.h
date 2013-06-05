#ifndef MONITOR_H
#define MONITOR_H
#include <types.h>

void monitor_put(char c);	// Write one char to the screen
void monitor_clear();		// Clears up the screen
void monitor_write(char *c);	// Output
void monitor_write_bin(uint32 x);	// Writes to the screen the bin version of the number
void monitor_write_hex(uint32 n);	// Writes to the screen the hex version of the number
void monitor_write_dec(uint32 n);	// Writes to the screen the dec version of the number
void monitor_write_linefeed();
#endif // MONITOR_H

