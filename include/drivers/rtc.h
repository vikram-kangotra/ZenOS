#pragma once

#include "stdint.h"
#include "stdbool.h"

// RTC ports
#define RTC_INDEX_PORT    0x70
#define RTC_DATA_PORT     0x71

// RTC registers
#define RTC_SECONDS       0x00
#define RTC_MINUTES       0x02
#define RTC_HOURS         0x04
#define RTC_DAY_OF_MONTH  0x07
#define RTC_MONTH         0x08
#define RTC_YEAR          0x09
#define RTC_STATUS_A      0x0A
#define RTC_STATUS_B      0x0B

// Status register B flags
#define RTC_24HOUR_FORMAT 0x02
#define RTC_BINARY_MODE   0x04

// Structure to hold date and time
struct DateTime {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;
    uint8_t month;
    uint8_t year;
};

// Initialize RTC
void rtc_init(void);

// Get current date and time
void rtc_get_time(struct DateTime* dt);

// Check if RTC is updating
bool rtc_is_updating(void);

// Read RTC register
uint8_t rtc_read_register(uint8_t reg);

// Convert BCD to binary
uint8_t bcd_to_binary(uint8_t bcd); 