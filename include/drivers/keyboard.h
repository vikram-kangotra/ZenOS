#pragma once

#include "arch/x86_64/interrupt/isr.h"

__attribute__((interrupt))
void irq_keyboard_handler(struct InterruptStackFrame* frame);

char buffer_read();
