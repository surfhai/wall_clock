// display.h — e-paper rendering (Waveshare 7.5" HAT, 800x480, GxEPD2)
//
// Layout is currently a simple placeholder (see CLAUDE.md section 10,
// "layout sketch ... still to be added as a reference file") and should be
// revisited once the final positioning is decided. Likewise, this currently
// uses built-in Adafruit GFX fonts until the custom font headers (see
// CLAUDE.md section 6) have been generated.
#pragma once

#include <Arduino.h>

// All text fields as fully formatted strings — main.cpp handles formatting
// (strftime etc.), display.cpp only takes care of drawing.
struct DisplayData {
    char timeStr[6];       // "12:05"
    char dateStr[11];      // "2026-07-30"
    char weekdayStr[16];   // "Donnerstag" / "Thursday"
    char weekStr[6];       // "KW32" / "CW32"
    char tempStr[8];       // "24,3°C"
    char humidityStr[8];   // "55%rLF" / "55%RH"
};

// Initializes SPI + the GxEPD2 display. Must be called once before any
// other display_* function.
void display_init();

// Full refresh (clears ghosting). Redraws all fields. Must be called on
// boot, after the ghosting threshold (see config.h) is reached, and after
// FULL_REFRESH_INTERVAL_MIN (see CLAUDE.md section 7).
void display_full_refresh(const DisplayData &data);

// Partial refresh of just the time field (the most frequent change).
// Increments the ghosting counter internally.
void display_partial_update_time(const char *timeStr);

// Partial refresh of the less frequently changing fields (date/week/weekday/
// sensor values). Increments the ghosting counter internally.
void display_partial_update_info(const DisplayData &data);

// Number of partial refreshes since the last full refresh.
uint16_t display_get_ghosting_counter();

// true once the ghosting counter has reached the threshold from config.h
// and the caller should force display_full_refresh().
bool display_needs_full_refresh();

// Puts the display into hibernation/low-power state (between updates,
// particularly relevant for later battery operation).
void display_hibernate();
