#pragma once

#include "stdint.h"

uint8_t in(uint16_t portnum);
void out(uint16_t portnum, uint8_t data);
void io_wait();
