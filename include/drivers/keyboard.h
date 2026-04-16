#pragma once
#include <stdint.h>

// Initialize the keyboard
void kb_init(void);

// Get a character from the keyboard
char kb_getchar(void);

// Check if a key is pressed
int kb_has_key(void);
