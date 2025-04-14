#include <string.h>

void *memset(void *s, int c, size_t n) {
    for (size_t i = 0; i < n; i++) {
        ((char *)s)[i] = (char)c;
    }
    return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
    for (size_t i = 0; i < n; i++) {
        ((char *)dest)[i] = ((char *)src)[i];
    }
    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    if (dest < src) {
        for (size_t i = 0; i < n; i++) {
            ((char *)dest)[i] = ((char *)src)[i];
        }
    } else if (dest > src) {
        for (size_t i = n; i > 0; i--) {
            ((char *)dest)[i - 1] = ((char *)src)[i - 1];
        }
    }
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;

    while (n-- > 0) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

size_t strlen(const char *s) {
    size_t len = 0;
    while (*s++) {
        len++;
    }
    return len;
}