// main.cpp — orchestration only (see CLAUDE.md section 4)
//
// Implements the deep-sleep tier cycle from CLAUDE.md section 5b: there is
// only one wakeup rhythm (every minute); hour/midnight changes are just
// minute changes where extra work happens. Each cycle reads the RTC time,
// does the tier's work, then computes exactly how long to sleep until the
// next minute boundary (minus a lead-time budget for that next tier's
// work) and arms the PCF8563 countdown timer for it.
//
// Currently (mains-powered, DEEP_SLEEP_ENABLED == 0) the same cycle runs
// from loop() with delay() instead of an actual deep sleep, so the timing
// logic is exercised and testable before battery operation is tackled (see
// CLAUDE.md section 5/10).
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
#if DISPLAY_LANGUAGE == LANG_DE
static const char *WEEKDAYS[7] = {
    "Sonntag", "Montag", "Dienstag", "Mittwoch",
    "Donnerstag", "Freitag", "Samstag"
};
static const char *WEEK_PREFIX = "KW";       // Kalenderwoche
static const char *HUMIDITY_SUFFIX = "rLF";  // relative Luftfeuchtigkeit
#else
static const char *WEEKDAYS[7] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
};
static const char *WEEK_PREFIX = "CW";       // Calendar Week
static const char *HUMIDITY_SUFFIX = "RH";   // Relative Humidity
#endif

enum class Tier { Minute, Hour, Midnight };

// If the RTC time deviates from the previously computed wakeup target by
// more than this, treat it as a missed cycle (e.g. power outage) and force
// a full refresh instead of trusting the normal tier logic (see CLAUDE.md
// section 7 "boot check for missed refreshes").
static const long kMissedCycleThresholdSeconds = 120;

static SensorReading s_lastDisplayedSensor{0.0f, 0.0f, false};

// Survives deep sleep (RTC_DATA_ATTR); 0 means "no expectation yet" (very
// first cycle since power-on). In mains-powered mode this is just a normal
// static, since loop() never actually resets.
RTC_DATA_ATTR static time_t s_expectedWakeupEpoch = 0;

static Tier tierForTime(const struct tm &t) {
    if (t.tm_hour == 0 && t.tm_min == 0) return Tier::Midnight;
    if (t.tm_min == 0) return Tier::Hour;
    return Tier::Minute;
}

static uint8_t budgetForTier(Tier tier) {
    switch (tier) {
        case Tier::Hour:     return TIER_BUDGET_HOUR_S;
        case Tier::Midnight: return TIER_BUDGET_MIDNIGHT_S;
        case Tier::Minute:
        default:             return TIER_BUDGET_MINUTE_S;
    }
}

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
        // Degree sign as the raw Latin-1 byte 0xB0, matching MonoText50.h's
        // character range (see include/fonts/README.md) - NOT a literal '°'
        // in the source, which would be UTF-8-encoded as two bytes
        // (0xC2 0xB0) and render as garbage, since GxEPD2/Adafruit_GFX reads
        // strings byte-by-byte and doesn't decode UTF-8.
        snprintf(out.tempStr, sizeof(out.tempStr), "%s%cC", tempBuf, (char)0xB0);
        snprintf(out.humidityStr, sizeof(out.humidityStr), "%d%%%s",
                  (int)lroundf(sensor.humidityPercent), HUMIDITY_SUFFIX);
    } else {
        snprintf(out.tempStr, sizeof(out.tempStr), "--,-%cC", (char)0xB0);
        snprintf(out.humidityStr, sizeof(out.humidityStr), "--%%%s", HUMIDITY_SUFFIX);
    }
}

// Runs exactly one tier cycle (CLAUDE.md section 5b): read time, do the
// tier's work, draw, then arm the countdown timer for the next minute
// boundary (or just delay() while mains-powered).
static void runCycle() {
    struct tm t{};
    getCurrentTime(t);
    time_t nowEpoch = mktime(&t);

    bool isFirstCycle = (s_expectedWakeupEpoch == 0);
    bool missedCycle = false;
    if (!isFirstCycle) {
        long delta = (long)(nowEpoch - s_expectedWakeupEpoch);
        if (delta < 0) delta = -delta;
        missedCycle = delta > kMissedCycleThresholdSeconds;
        if (missedCycle) {
            Serial.println("[main] Missed refresh cycle detected (RTC time far "
                            "from expected wakeup) - forcing full refresh.");
        }
    }

    Tier tier = tierForTime(t);

    // NTP sync happens once a day at the midnight tier (see CLAUDE.md
    // section 5b) - plus on the very first cycle after power-on, so the
    // clock doesn't run all day on a stale/default RTC time after a fresh
    // flash or dead RTC backup.
    if (tier == Tier::Midnight || isFirstCycle) {
        if (wifi_connect_or_configure()) {
            if (ntp_sync_time()) {
                struct tm synced;
                if (wifi_get_local_time(synced)) {
                    rtc_set_time(synced);
                    t = synced;
                    nowEpoch = mktime(&t);
                }
            } else if (rtc_lost_power()) {
                Serial.println("[main] WARNING: NTP failed and RTC backup is "
                                "empty - time may be wrong.");
            }
            wifi_disconnect();
        }
    }

    SensorReading sensor = sensor_read();
    bool sensorChanged = sensor_reading_changed_significantly(s_lastDisplayedSensor, sensor);

    DisplayData data{};
    formatDisplayData(t, sensor, data);

    bool forceFullRefresh = isFirstCycle || missedCycle || tier == Tier::Midnight ||
                             display_needs_full_refresh();
    if (forceFullRefresh) {
        display_full_refresh(data);
    } else if (tier == Tier::Hour) {
        display_partial_update_time(data.timeStr);
        display_partial_update_info(data);
    } else {
        display_partial_update_time(data.timeStr);
        if (sensorChanged) {
            display_partial_update_info(data);
        }
    }
    s_lastDisplayedSensor = sensor;

    // Next target is simply the next minute boundary (CLAUDE.md section 5b
    // step 4). Look ahead to that tier to know how much lead time to leave.
    time_t nextTargetEpoch = nowEpoch + 60;
    struct tm nextTargetTm;
    localtime_r(&nextTargetEpoch, &nextTargetTm);
    uint8_t budget = budgetForTier(tierForTime(nextTargetTm));

    long sleepSeconds = (long)(nextTargetEpoch - budget) - nowEpoch;
    if (sleepSeconds < 1) sleepSeconds = 1;  // never sleep negative/zero (section 5b safety net)
    if (sleepSeconds > RTC_TIMER_MAX_SECONDS) sleepSeconds = RTC_TIMER_MAX_SECONDS;

    s_expectedWakeupEpoch = nextTargetEpoch;

    if (DEEP_SLEEP_ENABLED) {
        rtc_set_countdown_timer((uint8_t)sleepSeconds);
        power_enter_deep_sleep();
    } else {
        delay((unsigned long)sleepSeconds * 1000UL);
    }
}

void setup() {
    Serial.begin(115200);
    delay(200);

    rtc_init();
    sensor_init();
    display_init();
    power_init_rtc_wakeup();

    if (power_woke_from_rtc_timer()) {
        rtc_clear_timer_flag();
    }

    runCycle();
}

void loop() {
    if (!DEEP_SLEEP_ENABLED) {
        runCycle();
    }
    // If DEEP_SLEEP_ENABLED, loop() is never reached - power_enter_deep_sleep()
    // in runCycle() restarts the next cycle via reset/wakeup instead.
}
