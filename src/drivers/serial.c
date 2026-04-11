#include <types.h>
#include <drivers/serial.h>
#include <shell/shell.h>
#include <arch/ports.h>

#define COM1 0x3F8

// Initialize COM1: 115200 baud, 8N1, enable RX interrupt
void serial_init(void) {
    outb(COM1 + 1, 0x00);    // Disable all interrupts during setup
    outb(COM1 + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(COM1 + 0, 0x01);    // Divisor low byte: 115200 baud
    outb(COM1 + 1, 0x00);    // Divisor high byte
    outb(COM1 + 3, 0x03);    // 8 bits, no parity, one stop bit (8N1)
    outb(COM1 + 2, 0xC7);    // Enable FIFO, clear, 14-byte threshold
    outb(COM1 + 4, 0x0B);    // IRQs enabled, RTS/DSR set (MCR)
    outb(COM1 + 1, 0x01);    // Enable RX data available interrupt (IER)
}

// Write one character to COM1, wait for transmit buffer empty
void serial_putc(char c) {
    while ((inb(COM1 + 5) & 0x20) == 0);   // Wait for THR empty (LSR bit 5)
    outb(COM1, c);
}

// Write null-terminated string to COM1
void serial_print(char *s) {
    while (*s) {
        serial_putc(*s);
        s++;
    }
}

// Called from ISR when COM1 receives data
void serial_driver(void) {
    while (inb(COM1 + 5) & 0x01) {         // Data available (LSR bit 0)
        char c = inb(COM1);
        if (c == '\r') c = '\n';            // Normalize CR to LF
        input_buf_push(c);
    }
}
