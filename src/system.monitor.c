#include <types.h>
#include <system.monitor.h>
#include <system.ports.h>

// The VGA framebuffer starts at 0xB8000.
static volatile uint16 *video_memory = (uint16 *)0xB8000;

// Stores the cursor position.
static uint8 cursor_x = 0;
static uint8 cursor_y = 24;

#define VIDEO_WIDTH   80
#define VIDEO_HEIGHT  24
#define VIDEO_ATTR  0x0A

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
    for( i=0; i< VIDEO_HEIGHT * VIDEO_WIDTH; i += 2) {
        video_memory[i] = 0x20 | (VIDEO_ATTR << 8);
    }
    cursor_x = 0;
    cursor_y = 0;
    update_cursor();
}

void scroll() {
    uint16 i;
    uint16 last_line = VIDEO_HEIGHT * VIDEO_WIDTH;

    for( i=0; i< last_line; i++)
        video_memory[i]   = video_memory[i+VIDEO_WIDTH];

    for( i=last_line; i< last_line+VIDEO_WIDTH; i++)
        video_memory[i]   = 0x20 | (VIDEO_ATTR << 8);

    cursor_x = 0;
    cursor_y = VIDEO_HEIGHT;
    update_cursor();
}

void putc(char c) {
    if(c == 0x0a){
        scroll();
        return;
    }

    uint16 position = (cursor_y * VIDEO_WIDTH) + cursor_x;
    video_memory[position] = c | (VIDEO_ATTR << 8);

    cursor_x++;
    if(cursor_x > VIDEO_WIDTH) {
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


