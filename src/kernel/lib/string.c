#include <string.h>
#include "string.h"
#include "kernel/mm/kmalloc.h"

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

char *strcpy(char *dest, const char *src) {
    char *original_dest = dest;
    while ((*dest++ = *src++) != '\0');
    return original_dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    char *original_dest = dest;
    while (n-- > 0 && (*dest++ = *src++) != '\0');
    return original_dest;
}

char *strchr(const char *s, int c) {
    while (*s != (char)c) {
        if (*s == '\0') {
            return NULL;
        }
        s++;
    }
    return (char *)s;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    while (*s) {
        if (*s == c) {
            last = s;
        }
        s++;
    }
    return (char *)last;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n-- > 0) {
        if (*s1 != *s2) {
            return *(unsigned char *)s1 - *(unsigned char *)s2;
        }
        if (*s1 == '\0') {
            return 0;
        }
        s1++;
        s2++;
    }
    return 0;
}

char *strcat(char *dest, const char *src) {
    char *original_dest = dest;
    while (*dest) {
        dest++;
    }

    while ((*dest++ = *src++) != '\0');
    return original_dest;
}

char *strncat(char *dest, const char *src, size_t n) {
    char *original_dest = dest;
    while (*dest) {
        dest++;
    }

    while (n-- > 0 && (*dest++ = *src++) != '\0');
    return original_dest;
}

char *strdup(const char *s) {
    size_t len = strlen(s);
    char *copy = kmalloc(len + 1);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, s, len + 1);
    return copy;
}

char *strndup(const char *s, size_t n) {
    size_t len = strlen(s);
    n = (n < len) ? n : len;
    char *copy = kmalloc(n + 1);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, s, n);
    copy[n] = '\0';
    return copy;
}

char *strtok(char *str, const char *delim) {
    static char *current = NULL;
    if (str) {
        current = str;
    }
    if (!current) {
        return NULL;
    }

    char *token = current;
    for (char *p = current; *p != '\0'; p++) {
        if (strchr(delim, *p)) {
            *p = '\0';
            current = p + 1;
            return token;
        }
    }

    current = NULL;
    return token;
}
