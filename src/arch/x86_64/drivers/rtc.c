#include "arch/x86_64/io.h"
#include "kernel/kprintf.h"
#include <stdint.h>

#define CMOS_ADDRESS 0x70
#define CMOS_DATA 0x71
#define TIMEZONE_OFFSET_HOURS 5
#define TIMEZONE_OFFSET_MINUTES 30

// Function to read a CMOS register
uint8_t read_rtc_register(uint8_t reg) {
    out(CMOS_ADDRESS, reg);
    return in(CMOS_DATA);
}

// Convert BCD to binary
uint8_t bcd_to_binary(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

// Function to adjust time for timezone offset
void adjust_timezone(uint8_t* hours, uint8_t* minutes) {
    *minutes += TIMEZONE_OFFSET_MINUTES;
    if (*minutes >= 60) {
        *minutes -= 60;
        (*hours)++;
    }

    *hours += TIMEZONE_OFFSET_HOURS;
    if (*hours >= 24) {
        *hours -= 24;
    }
}

// Function to read the RTC time
void read_rtc_time(uint8_t* hours, uint8_t* minutes, uint8_t* seconds) {
    // Wait for Update-In-Progress (UIP) bit to clear
    while (read_rtc_register(0x0A) & 0x80);

    // Read the time registers
    *seconds = read_rtc_register(0x00);
    *minutes = read_rtc_register(0x02);
    *hours   = read_rtc_register(0x04);

    // Read Register B to check if data is in BCD format
    uint8_t reg_b = read_rtc_register(0x0B);
    if (!(reg_b & 0x04)) {  // If not in binary mode, convert from BCD
        *seconds = bcd_to_binary(*seconds);
        *minutes = bcd_to_binary(*minutes);
        *hours   = bcd_to_binary(*hours);
    }

    // Adjust time for timezone offset
    adjust_timezone(hours, minutes);
}

// Function to print the current RTC time
void rtc_print_time() {
    uint8_t hours, minutes, seconds;

    read_rtc_time(&hours, &minutes, &seconds);

    kprintf(INFO, "Current Time: %d:%d:%d", hours, minutes, seconds);
}
