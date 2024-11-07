#include "print.h"

void isr_default_handler() {
    error("Unhandled interrupt\n");
    while (1);
}
