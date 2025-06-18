#include "drivers/rtc.h"
#include "arch/x86_64/io.h"

// Initialize RTC
void rtc_init(void) {
    // Read status register B
    uint8_t status = rtc_read_register(RTC_STATUS_B);
    
    // Set 24-hour format and binary mode
    status |= RTC_24HOUR_FORMAT | RTC_BINARY_MODE;
    
    // Write back to status register B
    outb(RTC_INDEX_PORT, RTC_STATUS_B);
    outb(RTC_DATA_PORT, status);
}

// Get current date and time
void rtc_get_time(struct DateTime* dt) {
    // Wait for RTC to not be updating
    while (rtc_is_updating());
    
    // Read all time registers
    dt->seconds = rtc_read_register(RTC_SECONDS);
    dt->minutes = rtc_read_register(RTC_MINUTES);
    dt->hours = rtc_read_register(RTC_HOURS);
    dt->day = rtc_read_register(RTC_DAY_OF_MONTH);
    dt->month = rtc_read_register(RTC_MONTH);
    dt->year = rtc_read_register(RTC_YEAR);
    
    // Convert from BCD to binary if needed
    uint8_t status = rtc_read_register(RTC_STATUS_B);
    if (!(status & RTC_BINARY_MODE)) {
        dt->seconds = bcd_to_binary(dt->seconds);
        dt->minutes = bcd_to_binary(dt->minutes);
        dt->hours = bcd_to_binary(dt->hours);
        dt->day = bcd_to_binary(dt->day);
        dt->month = bcd_to_binary(dt->month);
        dt->year = bcd_to_binary(dt->year);
    }
    
    // Add 5 hours and 30 minutes
    dt->minutes += 30;
    if (dt->minutes >= 60) {
        dt->minutes -= 60;
        dt->hours += 1;
    }
    
    dt->hours += 5;
    if (dt->hours >= 24) {
        dt->hours -= 24;
        dt->day += 1;
        
        // Handle month rollover
        int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        
        // Check for leap year
        if (dt->month == 2) {
            if ((dt->year % 4 == 0 && dt->year % 100 != 0) || (dt->year % 400 == 0)) {
                days_in_month[1] = 29; // February in leap year
            }
        }
        
        if (dt->day > days_in_month[dt->month - 1]) {
            dt->day = 1;
            dt->month += 1;
            
            // Handle year rollover
            if (dt->month > 12) {
                dt->month = 1;
                dt->year += 1;
            }
        }
    }
}

// Check if RTC is updating
bool rtc_is_updating(void) {
    outb(RTC_INDEX_PORT, RTC_STATUS_A);
    return inb(RTC_DATA_PORT) & 0x80;
}

// Read RTC register
uint8_t rtc_read_register(uint8_t reg) {
    outb(RTC_INDEX_PORT, reg);
    return inb(RTC_DATA_PORT);
}

// Convert BCD to binary
uint8_t bcd_to_binary(uint8_t bcd) {
    return ((bcd & 0xF0) >> 4) * 10 + (bcd & 0x0F);
} 