#include <types.h>
#include <drivers/monitor.h>
#include <drivers/serial.h>
#include <arch/ports.h>

#define VIDEO_WIDTH   80
#define VIDEO_HEIGHT  28
#define VIDEO_ATTR  0x0A

// The VGA framebuffer starts at 0xB8000.
static volatile uint16 *video_memory = (uint16 *)0xB8000;

// Stores the cursor position.
static uint8 cursor_x = 0;
static uint8 cursor_y = VIDEO_HEIGHT - 1;

void update_cursor()
{
   // The screen is 80 characters wide...
   uint16 cursorLocation = cursor_y * VIDEO_WIDTH + cursor_x;
   outb(0x03D4, 14);                  // Tell the VGA board we are setting the high cursor byte.
   outb(0x03D5, cursorLocation >> 8); // Send the high cursor byte.
   outb(0x03D4, 15);                  // Tell the VGA board we are setting the low cursor byte.
   outb(0x03D5, cursorLocation);      // Send the low cursor byte.
}

void cls() {
    int i;
    for( i=0; i< VIDEO_HEIGHT * VIDEO_WIDTH; i++) {
        video_memory[i] = 0x20 | (VIDEO_ATTR << 8);
    }
    cursor_x = 0;
    cursor_y = 0;
    update_cursor();
}

void scroll() {
    uint16 i;
    uint16 last_line = (VIDEO_HEIGHT - 1) * VIDEO_WIDTH;

    for( i=0; i< last_line; i++)
        video_memory[i]   = video_memory[i+VIDEO_WIDTH];

    for( i=last_line; i< last_line+VIDEO_WIDTH; i++)
        video_memory[i]   = 0x20 | (VIDEO_ATTR << 8);

    cursor_x = 0;
    cursor_y = VIDEO_HEIGHT - 1;
    update_cursor();
}

void putc(char c) {
    serial_putc(c);                     // Mirror all output to COM1

    if(c == 0x0a){
        cursor_x = 0;
        cursor_y++;
        if(cursor_y >= VIDEO_HEIGHT) {
            scroll();
        } else {
            update_cursor();
        }
        return;
    }

    if(c == 0x08) {  // Backspace
        if(cursor_x > 0) {
            cursor_x--;
            uint16 position = (cursor_y * VIDEO_WIDTH) + cursor_x;
            video_memory[position] = ' ' | (VIDEO_ATTR << 8);
            update_cursor();
        }
        return;
    }

    uint16 position = (cursor_y * VIDEO_WIDTH) + cursor_x;
    video_memory[position] = c | (VIDEO_ATTR << 8);

    cursor_x++;
    if(cursor_x >= VIDEO_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
   update_cursor();
    
}

void kprint(char *string) {
    while(*string != 0) {
        putc(*string);
        string++;
    }
}

void kprint_long2hex(long number, char* postString) {
    const uint8 hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    char *string = "0000000000000000h ";

    uint8 nibble;
    for(nibble = 16; nibble > 0; nibble--) { // 16 nibbles in a 64 bits number.
        string[nibble-1] = hex[number & 0xF];
        number = number >> 4;
    }
    kprint(string);
    kprint(postString);
}

// Print decimal number, right-aligned, padded with spaces to width
void kprint_dec_pad(uint64 num, uint32 width) {
    char buf[12];
    int len = 0;
    uint64 tmp = num;
    if (tmp == 0) { buf[len++] = '0'; }
    else { while (tmp) { buf[len++] = '0' + (tmp % 10); tmp /= 10; } }
    for (uint32 i = len; i < width; i++) putc(' ');
    for (int i = len - 1; i >= 0; i--) putc(buf[i]);
}

void kprint_dec(uint64 number) {
    if(number == 0) {
        putc('0');
        return;
    }
    
    char buffer[21];
    int i = 20;
    buffer[i] = '\0';
    
    while(number > 0 && i > 0) {
        i--;
        buffer[i] = '0' + (number % 10);
        number /= 10;
    }
    
    kprint(&buffer[i]);
}





