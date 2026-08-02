#include "net_time.h"
#include "config.h"

#include <WiFi.h>
#include <WiFiManager.h>
#include <Preferences.h>

namespace {

void saveWifiCredentials(const String &ssid, const String &pass) {
    Preferences prefs;
    prefs.begin(WIFI_PREFS_NAMESPACE, false);
    prefs.putString(WIFI_PREFS_KEY_SSID, ssid);
    prefs.putString(WIFI_PREFS_KEY_PASS, pass);
    prefs.end();
}

// Returns true if credentials were found in NVS.
bool loadWifiCredentials(String &ssid, String &pass) {
    Preferences prefs;
    prefs.begin(WIFI_PREFS_NAMESPACE, true);
    ssid = prefs.getString(WIFI_PREFS_KEY_SSID, "");
    pass = prefs.getString(WIFI_PREFS_KEY_PASS, "");
    prefs.end();
    return ssid.length() > 0;
}

// Optional first-boot bootstrap from build-time defaults, see config.h and
// CLAUDE.md section 5a. No-op unless DEFAULT_WIFI_SSID/DEFAULT_WIFI_PASS
// were supplied via build_flags.
void seedDefaultCredentialsIfEmpty() {
#if defined(DEFAULT_WIFI_SSID) && defined(DEFAULT_WIFI_PASS)
    String ssid, pass;
    if (!loadWifiCredentials(ssid, pass)) {
        Serial.println("[wifi] No stored credentials - seeding defaults from build_flags.");
        saveWifiCredentials(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASS);
    }
#endif
}

bool tryConnect(const String &ssid, const String &pass, uint32_t timeoutMs) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > timeoutMs) {
            return false;
        }
        delay(200);
    }
    return true;
}

}  // namespace

bool wifi_connect_or_configure() {
    seedDefaultCredentialsIfEmpty();

    String ssid, pass;
    bool connected = false;
    if (loadWifiCredentials(ssid, pass)) {
        connected = tryConnect(ssid, pass, WIFI_CONNECT_TIMEOUT_MS);
    }

    if (!connected) {
        // No stored credentials, or they no longer work (see CLAUDE.md
        // section 5a) -> open the captive portal.
        // TODO (CLAUDE.md section 10): also support a manual trigger
        // (button/boot-pin) to force this portal even with a working
        // connection, e.g. to move the clock to a new WiFi network.
        Serial.println("[wifi] No working connection - opening configuration AP.");
        WiFiManager wm;
        if (!wm.autoConnect(WIFI_CONFIG_AP_NAME, WIFI_CONFIG_AP_PASSWORD)) {
            Serial.println("[wifi] Configuration portal timed out/failed.");
            return false;
        }
        // WiFiManager already persisted the new credentials itself; mirror
        // them into our own Preferences namespace so loadWifiCredentials()
        // stays the single source of truth here.
        saveWifiCredentials(WiFi.SSID(), WiFi.psk());
        connected = true;
    }

    Serial.print("[wifi] Connected, IP: ");
    Serial.println(WiFi.localIP());
    return connected;
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
