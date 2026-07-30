// power.h — deep-sleep management, WiFi toggling, RTC wakeup
//
// Currently (mains-powered) effectively inactive (see config.h
// DEEP_SLEEP_ENABLED). Already prepared for later battery operation (see
// CLAUDE.md section 5) so main.cpp won't need to be restructured later.
#pragma once

#include <Arduino.h>

// Configures ext0 wakeup on PIN_RTC_INT (PCF8563 INT pin). Must run once
// before the first esp_deep_sleep_start() call.
// TODO: requires PIN_RTC_INT in config.h (currently a -1 placeholder).
void power_init_rtc_wakeup();

// Reason for the last wakeup (power-on, RTC alarm, etc.) — useful for the
// "boot check for missed refreshes" from CLAUDE.md section 7.
bool power_woke_from_rtc_alarm();

// Puts the ESP32 into deep sleep until the next PCF8563 alarm (via ext0
// wakeup), or — if DEEP_SLEEP_ENABLED == 0 — not at all (mains-powered,
// the function returns immediately and main.cpp continues via its
// delay() loop).
void power_enter_deep_sleep();
