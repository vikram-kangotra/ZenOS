#include "string.h"

uint8_t strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(uint8_t*)s1 - *(uint8_t*)s2;
}
size_t strlen(const char* str) {
    const char* s = str;
    while (*s) s++;
    return s - str;
}
char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}
char* strcat(char* dest, const char* src) {
    char* d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}
char* strncpy(char* dest, const char* src, size_t n) {
    char* d = dest;
    while (n && (*d++ = *src++)) n--;
    while (n--) *d++ = '\0';
    return dest;
}
int strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    return n ? (unsigned char)*s1 - (unsigned char)*s2 : 0;
}