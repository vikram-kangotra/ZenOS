#include "drivers/serial.h"
#include "arch/x86_64/io.h"
#include "stdint.h"

#define SERIAL_COM1_BASE    0x3F8

#define SERIAL_DATA_PORT(base)          (base)
#define SERIAL_INTERRUPT_PORT(base)     (base + 1)
#define SERIAL_FIFO_COMMAND_PORT(base)  (base + 2)
#define SERIAL_LINE_COMMAND_PORT(base)  (base + 3)
#define SERIAL_MODEM_COMMAND_PORT(base) (base + 4)
#define SERIAL_LINE_STATUS_PORT(base)   (base + 5)

#define SERIAL_LINE_ENABLE_DLAB     0X80

void serial_configure_interrupt(uint16_t com) {
    out(SERIAL_INTERRUPT_PORT(com), 0x00);
}

void serial_configure_baud_rate(uint16_t com, uint16_t divisor) {
    out(SERIAL_LINE_COMMAND_PORT(com), SERIAL_LINE_ENABLE_DLAB);
    out(SERIAL_DATA_PORT(com), (divisor >> 8) & 0x00FF);
    out(SERIAL_DATA_PORT(com), divisor & 0x00FF);
}

void serial_configure_line(uint16_t com) {
    out(SERIAL_LINE_COMMAND_PORT(com), 0X03);
}

void serial_configure_buffer(uint16_t com) {
    out(SERIAL_FIFO_COMMAND_PORT(com), 0xC7);
}

void serial_configure_modem(uint16_t com) {
    out(SERIAL_MODEM_COMMAND_PORT(com), 0X1E); // Set in loopback mode, test the serial chip
}

uint8_t serial_is_transmit_fifo_empty(uint16_t com) {
    return in(SERIAL_LINE_STATUS_PORT(com)) & 0x20;
}

int init_serial() {
    serial_configure_interrupt(SERIAL_COM1_BASE);

    serial_configure_baud_rate(SERIAL_COM1_BASE, 3);
    serial_configure_line(SERIAL_COM1_BASE);
    serial_configure_buffer(SERIAL_COM1_BASE);
    serial_configure_modem(SERIAL_COM1_BASE);
    
    out(SERIAL_DATA_PORT(SERIAL_COM1_BASE), 0xAE); // Test serial chip (send byte 0xAE and check if serial returns same byte)
    if (in(SERIAL_DATA_PORT(SERIAL_COM1_BASE)) != 0xAE) {
        return -1;
    }
    out(SERIAL_MODEM_COMMAND_PORT(SERIAL_COM1_BASE), 0x0F); // Test passed, (normal operation mode)
    return 0;
}

void serial_write_char(char ch) {
    while (!serial_is_transmit_fifo_empty(SERIAL_COM1_BASE));
    out(SERIAL_DATA_PORT(SERIAL_COM1_BASE), ch);
}
