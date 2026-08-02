// net_time.h — WiFi connection + NTP time sync
//
// Deliberately NOT called "wifi.h/.cpp" (as suggested in CLAUDE.md section
// 4): on Windows' case-insensitive filesystem, a local "wifi.h" collides
// with the <WiFi.h> include from the ESP32 Arduino core library, so its
// WiFi class can no longer be found (build error "'WiFi' was not declared
// in this scope").
//
// WiFi credentials are never hardcoded: they live in NVS (via Preferences)
// and are entered through a WiFiManager captive portal when no working
// connection is available (see CLAUDE.md section 5a).
//
// Only enabled briefly during the sync window (see CLAUDE.md section 5:
// toggling WiFi at runtime). Currently (mains-powered) WiFi simply stays on
// for simplicity; power.cpp will later take over targeted on/off switching
// for battery operation.
#pragma once

#include <Arduino.h>
#include <time.h>

// Tries to connect using the credentials stored in NVS. If that fails (or
// none are stored yet), opens the WiFiManager captive portal
// (WIFI_CONFIG_AP_NAME) so the user can enter new ones. Blocks until either
// connected or the portal times out. Returns true once connected.
bool wifi_connect_or_configure();

// Disconnects WiFi and powers down the WiFi modem (for the later
// low-power operation, see CLAUDE.md section 5).
void wifi_disconnect();

// Sets timezone + NTP servers (configTzTime) and waits until a plausible
// time has been received. Returns false if no sync succeeds within
// NTP_SYNC_TIMEOUT_MS (NTP fallback, see CLAUDE.md section 7 -> caller
// should then fall back to the PCF8563 time).
bool ntp_sync_time();

// Returns the current system time (set via time.h after a successful NTP
// sync). Returns false if the system time is obviously invalid (e.g.
// still at epoch).
bool wifi_get_local_time(struct tm &out);
