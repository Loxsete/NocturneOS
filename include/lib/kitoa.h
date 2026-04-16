#pragma once
#include <stdint.h>


// Convert an integer value to a string
void kitoa(uint32_t val, char *buf);

// Convert a hexadecimal value to a string
void kitoa_hex(uint64_t val, char *buf);
