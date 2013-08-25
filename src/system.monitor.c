#include <types.h>
#include <system.monitor.h>

// The VGA framebuffer starts at 0xB8000.
uint16 *video_memory = (uint16 *)0xB8A00;
// Stores the cursor position.
uint8 cursor_x = 0;
uint8 cursor_y = 0;

/////////////////////////////////////////////////////////////////
// Functions internal to system.monitor
// Not to be used from the external.


static void move_cursor()
{
   // The screen is 80 characters wide...
   uint16 cursorLocation = cursor_y * 80 + cursor_x;
   outb(0x3D4, 14);                  // Tell the VGA board we are setting the high cursor byte.
   outb(0x3D5, cursorLocation >> 8); // Send the high cursor byte.
   outb(0x3D4, 15);                  // Tell the VGA board we are setting the low cursor byte.
   outb(0x3D5, cursorLocation);      // Send the low cursor byte.
}

// Clears the screen, by copying lots of spaces to the framebuffer.
void monitor_clear()
{
   // Make an attribute byte for the default colours
   uint8 attributeByte = (0 /*black*/ << 4) | (15 /*white*/ & 0x0F);
   uint16 blank = 0x41 /* space */ | (attributeByte << 8);

   int i;
   for (i = 0; i < 80*25; i++)
   {
       video_memory[i] = blank;
   }

   // Move the hardware cursor back to the start.
   cursor_x = 0;
   cursor_y = 0;
   move_cursor();
}
