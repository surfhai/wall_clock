// config.h — central pin/constant definitions
// Source: CLAUDE.md sections 2-5. All values bundled here so pin or
// interval changes don't require touching multiple places in the code.
#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Display (SPI) — Waveshare 7.5" e-Paper HAT <-> Xiao ESP32-S3
// ---------------------------------------------------------------------------
#define PIN_EPD_CS   D0
#define PIN_EPD_DC   D1
#define PIN_EPD_RST  D9
#define PIN_EPD_BUSY D3   // requires the buzzer trace to have been cut
#define PIN_EPD_MOSI D10  // DIN
#define PIN_EPD_SCK  D8   // CLK
// No MISO needed, the display is write-only and never sends data back.

// ---------------------------------------------------------------------------
// SD card (expansion board, currently unused)
// ---------------------------------------------------------------------------
#define PIN_SD_CS D2

// ---------------------------------------------------------------------------
// I2C bus (shared by OLED, PCF8563 RTC, temperature/humidity sensor)
// ---------------------------------------------------------------------------
#define PIN_I2C_SDA D4  // GPIO5
#define PIN_I2C_SCL D5  // GPIO6

#define I2C_ADDR_OLED  0x3C
#define I2C_ADDR_RTC   0x51  // PCF8563

// TODO: PCF8563 INT pin -> ESP32 GPIO. Not yet documented/verified on the
// expansion board (see CLAUDE.md, open item). Without this pin the ext0
// wakeup in power.cpp will NOT work — check with a multimeter/schematic
// before commissioning and fill in here.
#define PIN_RTC_INT (-1)  // Placeholder, must be updated!

// ---------------------------------------------------------------------------
// Temperature/humidity sensor — final model not yet decided
// (candidates: SHT4x, SHT31, AHT20 — see CLAUDE.md section 2/10)
// Enable exactly one SENSOR_TYPE_* macro and uncomment the matching
// lib_deps line in platformio.ini.
// ---------------------------------------------------------------------------
#define SENSOR_TYPE_NONE  0  // Placeholder: returns dummy values, no sensor needed
#define SENSOR_TYPE_SHT4X 1
#define SENSOR_TYPE_SHT31 2
#define SENSOR_TYPE_AHT20 3

#ifndef SENSOR_TYPE
#define SENSOR_TYPE SENSOR_TYPE_NONE  // TODO: switch to the final sensor choice
#endif

// I2C addresses of the candidates (factory defaults):
#define I2C_ADDR_SHT4X 0x44
#define I2C_ADDR_SHT31 0x44  // may be 0x45 depending on the ADDR pin
#define I2C_ADDR_AHT20 0x38

// Hysteresis for sensor rounding: prevents constant redraws caused by
// sensor jitter (e.g. temperature flickering between 24.2/24.3 °C),
// see CLAUDE.md section 7.
#define TEMP_HYSTERESIS_C   0.2f
#define HUMIDITY_HYSTERESIS 1.0f

// ---------------------------------------------------------------------------
// WiFi / NTP
// ---------------------------------------------------------------------------
// Credentials are NOT hardcoded here. They live in NVS (via Preferences)
// and are entered through the WiFiManager captive portal, see CLAUDE.md
// section 5a. No more secrets.h.

// AP name shown for the WiFiManager configuration portal (opened when no
// stored/working WiFi connection is available).
#define WIFI_CONFIG_AP_NAME "Wall_Clock_Setup"

// Password for that AP. NOT hardcoded here - this file is committed to
// git, so a real password belongs in the gitignored wifi_defaults.ini
// (its WIFI_CONFIG_AP_PASSWORD build_flag, see wifi_defaults.ini.example),
// same mechanism as DEFAULT_WIFI_SSID/DEFAULT_WIFI_PASS above. This is
// just the fallback for when that file doesn't set one.
// WiFiManager requires at least 8 characters (WPA2 minimum) - anything
// shorter (including this empty-string fallback) means an open,
// unprotected AP.
#ifndef WIFI_CONFIG_AP_PASSWORD
#define WIFI_CONFIG_AP_PASSWORD ""
#endif

// Preferences (NVS) namespace + keys used to store the WiFi credentials
// that wifi_connect_or_configure() actually connects with.
#define WIFI_PREFS_NAMESPACE "wifi"
#define WIFI_PREFS_KEY_SSID  "ssid"
#define WIFI_PREFS_KEY_PASS  "pass"

// Optional first-boot bootstrap (see CLAUDE.md section 5a "Default-
// Zugangsdaten beim Firmware-Upload vorschreiben"): if DEFAULT_WIFI_SSID/
// DEFAULT_WIFI_PASS are defined (e.g. via build_flags from a gitignored
// wifi_defaults.ini, see platformio.ini), they seed NVS on first boot only,
// skipping the captive portal for initial commissioning.
// TODO (CLAUDE.md section 10): decide whether this is actually used, or
// first boot should always go through the AP fallback captive portal.

#define NTP_SERVER_PRIMARY   "de.pool.ntp.org"
#define NTP_SERVER_FALLBACK1 "pool.ntp.org"
#define NTP_SERVER_FALLBACK2 "time.cloudflare.com"

// POSIX TZ string for Europe/Berlin (CET/CEST), see CLAUDE.md section 4.
#define TZ_STRING "CET-1CEST,M3.5.0,M10.5.0/3"

// Maximum time to wait for the WiFi connection and the NTP response.
#define WIFI_CONNECT_TIMEOUT_MS 15000
#define NTP_SYNC_TIMEOUT_MS     10000

// Sync interval: once a day is sufficient (see CLAUDE.md section 4).
// TODO: revisit against the section 5 table once PCF8563 drift has been
// measured in practice.
#define TIME_SYNC_INTERVAL_HOURS 24

// ---------------------------------------------------------------------------
// Display output language (text rendered ON THE E-PAPER — not source code
// comments, those are always English). Only German and English are
// supported; German is the default per CLAUDE.md section 1.
// ---------------------------------------------------------------------------
#define LANG_DE 0
#define LANG_EN 1

#ifndef DISPLAY_LANGUAGE
#define DISPLAY_LANGUAGE LANG_DE
#endif

// ---------------------------------------------------------------------------
// Display refresh strategy (see CLAUDE.md section 7)
// ---------------------------------------------------------------------------
// Number of partial refreshes after which a full refresh is forced
// (ghosting reset). TODO: tune based on experience with the real display —
// 100 is a rough starting value (~once a day at a one-minute update rate).
#define GHOSTING_FULL_REFRESH_THRESHOLD 100

// ---------------------------------------------------------------------------
// Deep sleep / power (not actively used yet while mains-powered — see
// CLAUDE.md section 5, preparation for later battery operation)
// ---------------------------------------------------------------------------
#define DEEP_SLEEP_ENABLED 0  // TODO: set to 1 once battery operation is active

// The wall clock only ever wakes up once a minute (see CLAUDE.md section
// 5b) — hour/midnight changes are just minute changes where extra work
// happens. Wakeup is driven by the PCF8563's countdown TIMER register with
// a 1 Hz source (not the internal ESP32 timer, and not the PCF8563's alarm
// register — see CLAUDE.md section 5).
#define DISPLAY_UPDATE_INTERVAL_MIN 1

// Estimated lead time (in seconds) to wake up before each tier's target
// minute, so the display update finishes exactly on time (see CLAUDE.md
// section 5b, step 5). TODO: these are rough starting guesses — measure the
// actual duration of each tier's work (millis() before/after) on real
// hardware and replace with measured values plus a safety margin.
#define TIER_BUDGET_MINUTE_S    2
#define TIER_BUDGET_HOUR_S      3
#define TIER_BUDGET_MIDNIGHT_S 15

// PCF8563 timer register is 8-bit -> 255 s max at the 1 Hz source used here.
// Comfortably more than the ~60 s needed between wakeups (see CLAUDE.md
// section 5b).
#define RTC_TIMER_MAX_SECONDS 255
