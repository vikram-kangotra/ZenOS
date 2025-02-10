#pragma once

#include "kernel/mm/pmm.h"

void map_virtual_to_physical(uintptr_t virtual_address, uintptr_t physical_address);

uintptr_t virtual_to_physical(uintptr_t virtual_address);
