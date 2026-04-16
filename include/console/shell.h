#pragma once
#include <stdint.h>


// Draw the shell banner
void draw_banner(void);

// Run the shell
void shell_run(void);

// Execute a shell command
void shell_exec(const char *cmd);

// Display the shell prompt
void shell_prompt(void);
