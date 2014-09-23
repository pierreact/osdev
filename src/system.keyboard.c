
#include <system.monitor.h>
#include <types.h>

void keyboard_driver();

extern uint8 kbscancode;


#define SHIFT 0b0001
#define ALTGR 0b0010
#define CTRL  0b0100
#define ALT   0b1000


//bool shift = false;
#define RELEASE 0x80
uint8 kb_flags = 0;


void keyboard_driver() {
   
    char character = 0x00;
    if(kbscancode == 0x2A) kb_flags |= SHIFT; // Left Shift
    if(kbscancode == 0x36) kb_flags |= SHIFT; // Right Shift
    if(kbscancode == 0xAA) kb_flags &= ~SHIFT; // Release Left Shift
    if(kbscancode == 0xB6) kb_flags &= ~SHIFT; // Release Right Shift

    if((kb_flags & SHIFT) == SHIFT) {
        if(kbscancode == 0x3A) kb_flags &= ~SHIFT; // Remove caps lock
        if(kbscancode == 0x02) character = '!';
        if(kbscancode == 0x03) character = '"';
        if(kbscancode == 0x04) character = 0x9C;
        if(kbscancode == 0x05) character = '$';
        if(kbscancode == 0x06) character = '%';
        if(kbscancode == 0x07) character = '^';
        if(kbscancode == 0x08) character = '&';
        if(kbscancode == 0x09) character = '*';
        if(kbscancode == 0x0A) character = '(';
        if(kbscancode == 0x0B) character = ')';
        if(kbscancode == 0x0C) character = '_';
        if(kbscancode == 0x0D) character = '+';

        if(kbscancode == 0x10) character = 'Q';
        if(kbscancode == 0x11) character = 'W';
        if(kbscancode == 0x12) character = 'E';
        if(kbscancode == 0x13) character = 'R';
        if(kbscancode == 0x14) character = 'T';
        if(kbscancode == 0x15) character = 'Y';
        if(kbscancode == 0x16) character = 'U';
        if(kbscancode == 0x17) character = 'I';
        if(kbscancode == 0x18) character = 'O';
        if(kbscancode == 0x19) character = 'P';
        if(kbscancode == 0x1A) character = '{';
        if(kbscancode == 0x1B) character = '}';

        if(kbscancode == 0x1E) character = 'A';
        if(kbscancode == 0x1F) character = 'S';
        if(kbscancode == 0x20) character = 'D';
        if(kbscancode == 0x21) character = 'F';
        if(kbscancode == 0x22) character = 'G';
        if(kbscancode == 0x23) character = 'H';
        if(kbscancode == 0x24) character = 'J';
        if(kbscancode == 0x25) character = 'K';
        if(kbscancode == 0x26) character = 'L';
        if(kbscancode == 0x27) character = ':';
        if(kbscancode == 0x28) character = '@';

        if(kbscancode == 0x2B) character = '~';

        if(kbscancode == 0x56) character = '|';
        if(kbscancode == 0x2C) character = 'Z';
        if(kbscancode == 0x2D) character = 'X';
        if(kbscancode == 0x2E) character = 'C';
        if(kbscancode == 0x2F) character = 'V';
        if(kbscancode == 0x30) character = 'B';
        if(kbscancode == 0x31) character = 'N';
        if(kbscancode == 0x32) character = 'M';
        if(kbscancode == 0x33) character = '<';
        if(kbscancode == 0x34) character = '>';
        if(kbscancode == 0x35) character = '?';


    } else {
        if(kbscancode == 0x3A) kb_flags |= SHIFT; // Caps lock

        if(kbscancode == 0x02) character = '1';
        if(kbscancode == 0x03) character = '2';
        if(kbscancode == 0x04) character = '3';
        if(kbscancode == 0x05) character = '4';
        if(kbscancode == 0x06) character = '5';
        if(kbscancode == 0x07) character = '6';
        if(kbscancode == 0x08) character = '7';
        if(kbscancode == 0x09) character = '8';
        if(kbscancode == 0x0A) character = '9';
        if(kbscancode == 0x0B) character = '0';
        if(kbscancode == 0x0C) character = '-';
        if(kbscancode == 0x0D) character = '=';

        if(kbscancode == 0x10) character = 'q';
        if(kbscancode == 0x11) character = 'w';
        if(kbscancode == 0x12) character = 'e';
        if(kbscancode == 0x13) character = 'r';
        if(kbscancode == 0x14) character = 't';
        if(kbscancode == 0x15) character = 'y';
        if(kbscancode == 0x16) character = 'u';
        if(kbscancode == 0x17) character = 'i';
        if(kbscancode == 0x18) character = 'o';
        if(kbscancode == 0x19) character = 'p';
        if(kbscancode == 0x1A) character = '[';
        if(kbscancode == 0x1B) character = ']';

        if(kbscancode == 0x1E) character = 'a';
        if(kbscancode == 0x1F) character = 's';
        if(kbscancode == 0x20) character = 'd';
        if(kbscancode == 0x21) character = 'f';
        if(kbscancode == 0x22) character = 'g';
        if(kbscancode == 0x23) character = 'h';
        if(kbscancode == 0x24) character = 'j';
        if(kbscancode == 0x25) character = 'k';
        if(kbscancode == 0x26) character = 'l';
        if(kbscancode == 0x27) character = ';';
        if(kbscancode == 0x28) character = '\'';

        if(kbscancode == 0x2B) character = '#';

        if(kbscancode == 0x56) character = '\\';
        if(kbscancode == 0x2C) character = 'z';
        if(kbscancode == 0x2D) character = 'x';
        if(kbscancode == 0x2E) character = 'c';
        if(kbscancode == 0x2F) character = 'v';
        if(kbscancode == 0x30) character = 'b';
        if(kbscancode == 0x31) character = 'n';
        if(kbscancode == 0x32) character = 'm';
        if(kbscancode == 0x33) character = ',';
        if(kbscancode == 0x34) character = '.';
        if(kbscancode == 0x35) character = '/';

        if(kbscancode == 0x39) character = ' ';
        if(kbscancode == 0x1C) character = 0x0A; // New line, return key.
    }
    if(character != 0x0) putc(character);
}

