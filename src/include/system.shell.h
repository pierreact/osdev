#ifndef SYSTEM_SHELL_H
#define SYSTEM_SHELL_H

#include <types.h>

// Shell initialization - clears screen and shows welcome message
void shell_init();

// Display command prompt
void shell_prompt();

// Handle incoming character from keyboard
void shell_handle_char(char c);

// Execute command in buffer
void shell_execute_command();

#endif // SYSTEM_SHELL_H
