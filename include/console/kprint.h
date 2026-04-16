#pragma once
#include <stdint.h>


// Print a string
void kputs(const char *s);

// Print a string with a specific foreground color
void kputs_col(const char *s, uint32_t fg);

// Print a hexadecimal number
void kputhex(uint64_t val);

// Print a decimal number
void kputdec(uint64_t val);


#define COLOR_BG     0x0D0D1A
#define COLOR_FG     0xFFFFFF
#define COLOR_PROMPT 0x00CC44
#define COLOR_INPUT  0xFFFFFF
#define COLOR_DIR    0x4488FF
