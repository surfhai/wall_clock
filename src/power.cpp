#include "power.h"
#include "config.h"

#include <esp_sleep.h>

void power_init_rtc_wakeup() {
#if DEEP_SLEEP_ENABLED
    if (PIN_RTC_INT < 0) {
        Serial.println("[power] PIN_RTC_INT not set - ext0 wakeup cannot "
                        "be configured (see config.h TODO).");
        return;
    }
    // PCF8563 INT is typically active-low (open-drain) -> wake on LOW.
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_RTC_INT, 0);
#endif
}

bool power_woke_from_rtc_alarm() {
    return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0;
}

void power_enter_deep_sleep() {
#if DEEP_SLEEP_ENABLED
    Serial.println("[power] Entering deep sleep until the next PCF8563 alarm.");
    Serial.flush();
    esp_deep_sleep_start();
#else
    // Mains-powered: no deep sleep, main.cpp actively waits instead (see
    // main.cpp loop) until the next update cycle.
#endif
}
