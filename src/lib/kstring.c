#include <lib/kstring.h>

int kstrlen(const char *s)
{
    int n = 0; while (s[n]) n++; return n;
}

int kstrcmp(const char *a, const char *b)
{
    while (*a && *b && *a == *b) { a++; b++; }
    return *a - *b;
}

int kstrncmp(const char *a, const char *b, int n)
{
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) return a[i] - b[i];
        if (!a[i]) return 0;
    }
    return 0;
}

void kstrcpy(char *dst, const char *src, int max)
{
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

void kmemcpy(void *dst, const void *src, uint64_t n)
{
    uint8_t *d = dst; const uint8_t *s = src;
    for (uint64_t i = 0; i < n; i++) d[i] = s[i];
}

void kmemset(void *dst, uint8_t val, uint64_t n)
{
    uint8_t *d = dst;
    for (uint64_t i = 0; i < n; i++) d[i] = val;
}