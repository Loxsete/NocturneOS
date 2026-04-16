#pragma once
#include <stdint.h>

// Switch to user mode
void jump_usermode(uint64_t entry, uint64_t stack);
