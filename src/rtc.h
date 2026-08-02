// rtc.h — access to the external PCF8563 RTC (expansion board, I2C 0x51)
//
// The PCF8563 is the actual timekeeping source during (later) deep-sleep
// operation (see CLAUDE.md section 5) and serves as a fallback if no NTP
// server is reachable at boot (see CLAUDE.md section 7).
#pragma once

#include <Arduino.h>
#include <time.h>

// Initializes the I2C bus (if not already done) and the PCF8563.
// Returns false if the RTC was not found.
bool rtc_init();

// true if the RTC has been completely without power since the time was last
// set (e.g. empty backup battery/capacitor) -> stored time is invalid.
bool rtc_lost_power();

// Reads the current time from the PCF8563.
bool rtc_get_time(struct tm &out);

// Writes a time (e.g. after a successful NTP sync) to the PCF8563.
void rtc_set_time(const struct tm &t);

// Arms the PCF8563's countdown TIMER register (not the alarm register, see
// CLAUDE.md section 5) with a 1 Hz source, so it fires an interrupt in
// exactly `seconds` seconds (max RTC_TIMER_MAX_SECONDS, since the register
// is 8-bit). RTClib's RTC_PCF8563 class does not expose the timer, so this
// talks to the chip's timer registers directly over I2C.
// TODO (battery operation, currently unused while DEEP_SLEEP_ENABLED == 0):
// PIN_RTC_INT must be finalized first (see config.h TODO) for the ESP32 to
// actually wake up from this interrupt via ext0.
void rtc_set_countdown_timer(uint8_t seconds);

// Clears the timer interrupt flag on the PCF8563 (call after waking up, so
// the next rtc_set_countdown_timer() call can trigger INT again).
void rtc_clear_timer_flag();
