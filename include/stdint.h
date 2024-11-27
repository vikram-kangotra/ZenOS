#pragma once

typedef signed char int8_t;
typedef short int int16_t;
typedef int int32_t;
typedef long long int int64_t;

typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long int uint64_t;
typedef unsigned long size_t;

#if defined(__x86_64__) || defined(_WIN64)  // 64-bit architecture
    typedef unsigned long long uintptr_t;
#elif defined(__i386__) || defined(_WIN32)  // 32-bit architecture
    typedef unsigned int uintptr_t;
#else
    #error "Unsupported platform"
#endif
