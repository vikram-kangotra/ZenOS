#pragma once

inline void sti() {
    asm volatile("sti");
}
