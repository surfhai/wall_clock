#include "rtc.h"
#include "config.h"

#include <Wire.h>
#include <RTClib.h>

static RTC_PCF8563 pcf8563;
static bool s_initialized = false;

// PCF8563 register addresses/bits for the countdown timer (NXP PCF8563
// datasheet). Not exposed by RTClib's RTC_PCF8563 class, so accessed
// directly here.
namespace {
constexpr uint8_t kRegControlStatus2 = 0x01;
constexpr uint8_t kRegTimerControl   = 0x0E;
constexpr uint8_t kRegTimer          = 0x0F;

constexpr uint8_t kCtrl2Tie = 1 << 2;  // Timer Interrupt Enable
constexpr uint8_t kTimerCtrlEnable  = 1 << 7;  // TE
constexpr uint8_t kTimerCtrlFd1Hz  = 0b10;     // TD1:TD0 = 1 Hz source

void writeReg(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(I2C_ADDR_RTC);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}
}  // namespace

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

void rtc_set_countdown_timer(uint8_t seconds) {
    if (!s_initialized) return;

    // Disable the timer first while reconfiguring, then load the countdown
    // value, then enable with a 1 Hz source (see CLAUDE.md section 5 -
    // timer register, not the alarm register).
    writeReg(kRegTimerControl, kTimerCtrlFd1Hz);
    writeReg(kRegTimer, seconds);
    writeReg(kRegTimerControl, kTimerCtrlEnable | kTimerCtrlFd1Hz);

    // Enable the timer interrupt and clear any pending timer/alarm flags.
    writeReg(kRegControlStatus2, kCtrl2Tie);
}

void rtc_clear_timer_flag() {
    if (!s_initialized) return;
    // Rewriting CTRL2 with TF/AF at 0 clears both flags while keeping TIE
    // enabled for the next countdown.
    writeReg(kRegControlStatus2, kCtrl2Tie);
}
