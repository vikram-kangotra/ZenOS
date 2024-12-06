#pragma once

#include "stdint.h"

extern void cli();
extern void sti();
extern void lgdt();
extern void ltr();
extern void lidt(uint64_t idtp);
extern void load_cr3(uint64_t pml4_address);
extern void enable_paging();
