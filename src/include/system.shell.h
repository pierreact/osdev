#ifndef SYSTEM_SHELL_H
#define SYSTEM_SHELL_H

#include <types.h>

// Shell initialization - clears screen and shows welcome message
void shell_init();

// Display command prompt
void shell_prompt();

// Handle incoming character from keyboard (ring 0 direct call, used before ring 3)
void shell_handle_char(char c);

// Input ring buffer push (called from ISR, feeds ring 3 shell via syscall)
void input_buf_push(char c);

// Ring 3 entry point for the shell task
void shell_ring3_entry(void);

// Execute command in buffer
void shell_execute_command();

#endif // SYSTEM_SHELL_H
