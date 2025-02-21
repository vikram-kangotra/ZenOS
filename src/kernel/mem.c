#include "stddef.h"
#include "stdint.h"
#include "kernel/mem.h"

void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    size_t k;

    if (!n) return dest;
    d[0] = s[0];
    d[n - 1] = s[n - 1];

    if (n <= 2) return dest;
    d[1] = s[1];
    d[n - 2] = s[n - 2];

    if (n <= 6) return dest;
    d[2] = s[2];
    d[n - 3] = s[n - 3];

    if (n <= 8) return dest;
    d[3] = s[3];
    d[n - 4] = s[n - 4];

    k = -(uintptr_t)d & 3;
    d += k;
    s += k;
    n -= k;

    size_t word_count = n / 4;
    uint32_t *wd = (uint32_t *)d;
    const uint32_t *ws = (const uint32_t *)s;

    for (; word_count; word_count--, wd++, ws++) {
        *wd = *ws;
    }

    d = (unsigned char *)wd;
    s = (const unsigned char *)ws;
    n &= 3;

    for (; n; n--, d++, s++) {
        *d = *s;
    }

    return dest;
}

void * memset(void * dest, int c, size_t n)
{
    unsigned char *s = dest;
    size_t k;

    if (!n) return dest;
    s[0] = s[n-1] = c;
    if (n <= 2) return dest;
    s[1] = s[n-2] = c;
    s[2] = s[n-3] = c;
    if (n <= 6) return dest;
    s[3] = s[n-4] = c;
    if (n <= 8) return dest;

    k = -(uintptr_t)s & 3;
    s += k;
    n -= k;
    n &= -4;
    n /= 4;

    uint32_t *ws = (uint32_t *)s;
    uint32_t wc = c & 0xFF;
    wc |= ((wc << 8) | (wc << 16) | (wc << 24));

    for (; n; n--, ws++) *ws = wc;

    return dest;
}