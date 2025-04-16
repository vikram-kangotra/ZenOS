#pragma once

#include "stdint.h"

uint8_t inb(uint16_t portnum);
void outb(uint16_t portnum, uint8_t data);
void io_wait();
