#pragma once

#include "stdint.h"

extern void cli();
extern void sti();
extern void lgdt();
extern void ltr();
extern void lidt(uint64_t idtp);
extern void invlpg(uintptr_t ptr);
extern uintptr_t get_current_pml4();
extern uintptr_t get_faulting_address();
