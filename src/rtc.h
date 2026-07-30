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

// TODO (battery operation, currently unused while DEEP_SLEEP_ENABLED == 0):
// set an alarm for the next wakeup time (minute resolution is enough for
// the display update cycle, see config.h DISPLAY_UPDATE_INTERVAL_MIN).
// PIN_RTC_INT must be finalized first (see config.h TODO). Check the exact
// alarm API against the actually installed RTClib version (Adafruit
// RTClib, class RTC_PCF8563), as method names can differ between versions.
void rtc_set_next_wakeup_alarm(uint8_t minutesFromNow);

// Clears the alarm flag on the PCF8563 (call after waking up).
void rtc_clear_alarm();
