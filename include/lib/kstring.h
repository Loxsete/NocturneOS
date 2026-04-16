#pragma once
#include <stdint.h>

// Get the length of a string
int kstrlen(const char *s);

// Compare two strings
int kstrcmp(const char *a, const char *b);

// Compare two strings up to n characters
int kstrncmp(const char *a, const char *b, int n);

// Copy a string
void kstrcpy(char *dst, const char *src, int max);

// Copy memory
void kmemcpy(void *dst, const void *src, uint64_t n);

// Set memory
void kmemset(void *dst, uint8_t val, uint64_t n);