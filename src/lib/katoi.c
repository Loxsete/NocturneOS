#include <lib/katoi.h>

int katoi(const char *s)
{
    int n = 0;

    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }

    return n;
}
