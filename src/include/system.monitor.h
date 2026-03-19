#ifndef MONITOR_H
#define MONITOR_H
#include <types.h>
void update_cursor();
void cls();		// Clears up the screen
void scroll();
void putc(char c);	// Write one char to the screen

void kprint(char *string);
void kprint_long2hex(long number, char* postString);
void kprint_dec(uint64 number);
void kprint_dec_pad(uint64 num, uint32 width);

#endif // MONITOR_H

