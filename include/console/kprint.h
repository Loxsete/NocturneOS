#pragma once
#include <stdint.h>

void kputdec(uint64_t val);
void kputhex(uint64_t val);
void kputs_col(const char *s, uint32_t fg);
void kputs(const char *s);

#define COLOR_BG     0x0D0D1A
#define COLOR_FG     0xFFFFFF
#define COLOR_PROMPT 0x00CC44   
#define COLOR_INPUT  0xFFFFFF
#define COLOR_DIR    0x4488FF   
