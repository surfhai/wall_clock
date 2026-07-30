// main.cpp — orchestration only (see CLAUDE.md section 4)
//
// Current mode of operation: mains-powered, no deep sleep (see config.h
// DEEP_SLEEP_ENABLED). The deep-sleep/RTC-wakeup paths are already wired up
// but untested until battery operation is tackled (see CLAUDE.md section 5/10).
#include <Arduino.h>
#include <time.h>
#include <math.h>

#include "config.h"
#include "rtc.h"
#include "sensor.h"
#include "net_time.h"
#include "display.h"
#include "power.h"

// Localized strings actually rendered on the e-paper display. Only German
// (default) and English are supported, selected via DISPLAY_LANGUAGE in
// config.h — everything else in this codebase (comments, logs) is English
// regardless of this setting.
#if DISPLAY_LANGUAGE == LANG_EN
static const char *WEEKDAYS[7] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
};
static const char *WEEK_PREFIX = "CW";       // Calendar Week
static const char *HUMIDITY_SUFFIX = "RH";   // Relative Humidity
#else
static const char *WEEKDAYS[7] = {
    "Sonntag", "Montag", "Dienstag", "Mittwoch",
    "Donnerstag", "Freitag", "Samstag"
};
static const char *WEEK_PREFIX = "KW";       // Kalenderwoche
static const char *HUMIDITY_SUFFIX = "rLF";  // relative Luftfeuchtigkeit
#endif

static SensorReading s_lastDisplayedSensor{0.0f, 0.0f, false};
static int s_lastDisplayedMinute = -1;
static unsigned long s_lastNtpSyncMillis = 0;

// Fetches the current time: prefers system time (after NTP sync), falls
// back to the PCF8563 otherwise (see CLAUDE.md section 7 "NTP fallback").
static bool getCurrentTime(struct tm &out) {
    if (wifi_get_local_time(out)) {
        return true;
    }
    Serial.println("[main] No valid system time - falling back to PCF8563.");
    return rtc_get_time(out);
}

static void formatDisplayData(const struct tm &t, const SensorReading &sensor,
                               DisplayData &out) {
    strftime(out.timeStr, sizeof(out.timeStr), "%H:%M", &t);
    strftime(out.dateStr, sizeof(out.dateStr), "%Y-%m-%d", &t);
    snprintf(out.weekdayStr, sizeof(out.weekdayStr), "%s",
             WEEKDAYS[t.tm_wday]);

    // ISO calendar week via %V. TODO (CLAUDE.md section 4): verify against
    // the actually used Arduino core/newlib version before going live.
    char isoWeek[4];
    strftime(isoWeek, sizeof(isoWeek), "%V", &t);
    snprintf(out.weekStr, sizeof(out.weekStr), "%s%s", WEEK_PREFIX, isoWeek);

    if (sensor.valid) {
        // German decimal comma instead of a dot, per CLAUDE.md section 1
        // ("24,3°C") — this is the display format, independent of
        // DISPLAY_LANGUAGE.
        char tempBuf[8];
        snprintf(tempBuf, sizeof(tempBuf), "%.1f", sensor.temperatureC);
        for (char *p = tempBuf; *p; ++p) {
            if (*p == '.') *p = ',';
        }
        // TODO (CLAUDE.md section 6): use "°C" instead of "C" once the
        // custom fonts include the degree sign (outside the 7-bit ASCII
        // range).
        snprintf(out.tempStr, sizeof(out.tempStr), "%sC", tempBuf);
        snprintf(out.humidityStr, sizeof(out.humidityStr), "%d%%%s",
                  (int)lroundf(sensor.humidityPercent), HUMIDITY_SUFFIX);
    } else {
        snprintf(out.tempStr, sizeof(out.tempStr), "--,-C");
        snprintf(out.humidityStr, sizeof(out.humidityStr), "--%%%s", HUMIDITY_SUFFIX);
    }
}

void setup() {
    Serial.begin(115200);
    delay(200);

    rtc_init();
    sensor_init();
    display_init();
    power_init_rtc_wakeup();

    bool haveTime = false;

    if (wifi_connect()) {
        if (ntp_sync_time()) {
            struct tm t;
            if (wifi_get_local_time(t)) {
                rtc_set_time(t);
                haveTime = true;
            }
            s_lastNtpSyncMillis = millis();
        }
        wifi_disconnect();
    }

    if (!haveTime && rtc_lost_power()) {
        Serial.println("[main] WARNING: RTC has no valid time "
                        "(NTP failed and RTC backup is empty).");
    }

    struct tm t{};
    getCurrentTime(t);
    SensorReading sensor = sensor_read();
    s_lastDisplayedSensor = sensor;

    DisplayData data{};
    formatDisplayData(t, sensor, data);
    display_full_refresh(data);
    s_lastDisplayedMinute = t.tm_min;

    if (DEEP_SLEEP_ENABLED) {
        power_enter_deep_sleep();
    }
}

void loop() {
    if (DEEP_SLEEP_ENABLED) {
        // Should never be reached - power_enter_deep_sleep() in setup()
        // restarts the next cycle via reset/wakeup.
        return;
    }

    struct tm t{};
    if (!getCurrentTime(t)) {
        delay(1000);
        return;
    }

    // Periodic NTP resync (see config.h TIME_SYNC_INTERVAL_HOURS).
    unsigned long hoursSinceSync =
        (millis() - s_lastNtpSyncMillis) / (3600UL * 1000UL);
    if (hoursSinceSync >= TIME_SYNC_INTERVAL_HOURS) {
        if (wifi_connect()) {
            if (ntp_sync_time()) {
                struct tm synced;
                if (wifi_get_local_time(synced)) {
                    rtc_set_time(synced);
                }
                s_lastNtpSyncMillis = millis();
            }
            wifi_disconnect();
        }
    }

    if (t.tm_min != s_lastDisplayedMinute) {
        SensorReading sensor = sensor_read();
        bool sensorChanged = sensor_reading_changed_significantly(
            s_lastDisplayedSensor, sensor);

        DisplayData data{};
        formatDisplayData(t, sensor, data);

        if (display_needs_full_refresh()) {
            display_full_refresh(data);
        } else {
            display_partial_update_time(data.timeStr);
            if (sensorChanged || t.tm_min == 0) {
                // Time field every minute, info/sensor line only on change
                // or once per hour.
                display_partial_update_info(data);
            }
        }

        if (sensorChanged) {
            s_lastDisplayedSensor = sensor;
        }
        s_lastDisplayedMinute = t.tm_min;
    }

    delay(1000);
}
