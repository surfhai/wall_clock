#include "net_time.h"
#include "config.h"
#include "secrets.h"

#include <WiFi.h>

bool wifi_connect() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
            Serial.println("[wifi] Connection failed (timeout).");
            return false;
        }
        delay(200);
    }

    Serial.print("[wifi] Connected, IP: ");
    Serial.println(WiFi.localIP());
    return true;
}

void wifi_disconnect() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

bool ntp_sync_time() {
    // Primary/fallback NTP servers + timezone, see CLAUDE.md section 4.
    configTzTime(TZ_STRING, NTP_SERVER_PRIMARY, NTP_SERVER_FALLBACK1,
                 NTP_SERVER_FALLBACK2);

    struct tm timeinfo;
    uint32_t start = millis();
    while (!getLocalTime(&timeinfo, 0)) {
        if (millis() - start > NTP_SYNC_TIMEOUT_MS) {
            Serial.println("[wifi] NTP sync failed (timeout). "
                            "Caller should fall back to the PCF8563 time.");
            return false;
        }
        delay(200);
    }

    Serial.println("[wifi] NTP sync successful.");
    return true;
}

bool wifi_get_local_time(struct tm &out) {
    if (!getLocalTime(&out, 0)) {
        return false;
    }
    // Before NTP sync/RTC takeover, the system time is typically at 1970
    // (epoch) -> treat as invalid.
    return (out.tm_year + 1900) > 2000;
}
