#include <stdint.h>

void kitoa(uint32_t n, char *buf)
{
    if (n == 0) { buf[0] = '0'; buf[1] = 0; return; }
    char tmp[12]; int i = 0;
    while (n) { tmp[i++] = '0' + (n % 10); n /= 10; }
    int j = 0;
    for (int k = i - 1; k >= 0; k--) buf[j++] = tmp[k];
    buf[j] = 0;
}

void kitoa_hex(uint64_t val, char *buf)
{
    const char *hex = "0123456789ABCDEF";
    int i = 15;
    buf[16] = '\0';
    while (i >= 0) {
        buf[i--] = hex[val & 0xF];
        val >>= 4;
    }
}
