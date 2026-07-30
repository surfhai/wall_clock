#include "rtc.h"
#include "config.h"

#include <Wire.h>
#include <RTClib.h>

static RTC_PCF8563 pcf8563;
static bool s_initialized = false;

bool rtc_init() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    s_initialized = pcf8563.begin(&Wire);
    if (!s_initialized) {
        Serial.println("[rtc] PCF8563 not found!");
    }
    return s_initialized;
}

bool rtc_lost_power() {
    if (!s_initialized) return true;
    return pcf8563.lostPower();
}

bool rtc_get_time(struct tm &out) {
    if (!s_initialized) return false;

    DateTime now = pcf8563.now();
    out.tm_year = now.year() - 1900;
    out.tm_mon  = now.month() - 1;
    out.tm_mday = now.day();
    out.tm_hour = now.hour();
    out.tm_min  = now.minute();
    out.tm_sec  = now.second();
    out.tm_isdst = -1;
    mktime(&out);  // normalizes tm_wday/tm_yday among others
    return true;
}

void rtc_set_time(const struct tm &t) {
    if (!s_initialized) return;

    DateTime dt(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                t.tm_hour, t.tm_min, t.tm_sec);
    pcf8563.adjust(dt);
}

void rtc_set_next_wakeup_alarm(uint8_t minutesFromNow) {
    // TODO (battery operation): check the RTClib::RTC_PCF8563 alarm API and
    // use it here (e.g. set a minute/hour alarm to now()+minutesFromNow).
    // Currently unused since DEEP_SLEEP_ENABLED == 0 (mains-powered).
    (void)minutesFromNow;
}

void rtc_clear_alarm() {
    // TODO (battery operation): implement the alarm flag reset according to
    // the RTClib version (see rtc_set_next_wakeup_alarm).
}
