#pragma once

#include "idt.h"

void page_fault_handler(struct InterruptData *data, struct InterruptStackFrame *frame);
