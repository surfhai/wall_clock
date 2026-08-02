# CLAUDE.md — DIY e-Paper Wanduhr (ESP32-S3)

Diese Datei enthält alle bisher besprochenen und festgelegten Spezifikationen für das Projekt.
Sie dient als Kontext für Claude Code in VS Code, damit Architektur- und Hardware-Entscheidungen
nicht in jeder neuen Session erneut erklärt werden müssen.

---

## 1. Projektübersicht

DIY-Wanduhr auf Basis eines e-Paper-Displays, die folgende Informationen anzeigt:

- **Uhrzeit** — Format `12:05`
- **Kalenderwoche (ISO)** — Format `KW32`
- **Wochentag** (deutsch) — z. B. `Donnerstag`
- **Datum** — Format `2026-07-30`
- **Temperatur** — z. B. `24,3°C` (eine Nachkommastelle)
- **relative Luftfeuchtigkeit** — z. B. `55%rLF`

Temperatur/Luftfeuchte werden von einem I2C-Sensor am Grove-Anschluss des Expansion Boards gelesen.

**Sprache der Anzeige:** Standardmäßig Deutsch (wie oben, `DISPLAY_LANGUAGE` = `LANG_DE`
in `include/config.h`). Zusätzlich existiert **eine** Alternative, Englisch (`LANG_EN`) —
bewusst kein generisches Mehrsprachen-System, sondern genau dieser eine Schalter. Betroffen
sind nur die auf dem Display gerenderten Texte (Wochentagsnamen, `KW`/`CW`-Präfix,
`rLF`/`RH`-Suffix), formatiert in `src/main.cpp`. Der Quellcode selbst (Kommentare, Log-
Ausgaben) ist davon unabhängig immer Englisch, siehe Abschnitt 11.

**Betriebsart:** Aktuell dauerhaft am Netzteil betrieben. Batteriebetrieb ist ein späteres Ziel
(alle Energiespar-Entscheidungen unten sind bereits im Hinblick darauf getroffen, aber aktuell
nicht zwingend erforderlich, solange am Netzteil betrieben wird).

---

## 2. Hardware

| Komponente | Modell / Artikelnummer |
|---|---|
| Display | Waveshare 7,5" e-Paper HAT, schwarz/weiß, **Artikelnummer 013504**, **V2-Variante (800×480, seit Sept. 2023)**, 170° Sichtwinkel, direkte SPI-Ansteuerung (**keine** IT8951-Variante). Panel-Aufdruck auf der FPC-Kabelseite: `075BN-T7-D2 N2A4P05` → bestätigt `GxEPD2_750_T7` (T7-Controller, siehe Abschnitt 4). |
| Microcontroller | Seeed XIAO ESP32-S3 (WiFi/Bluetooth) |
| Erweiterung | Seeeduino XIAO Expansion Board (mit onboard OLED, RTC PCF8563, SD-Kartenslot, Grove-I2C-Anschlüssen) |
| Externe RTC | PCF8563 (auf dem Expansion Board), I2C-Adresse 0x51 |
| Sensor (Temp/Feuchte) | **Noch nicht final entschieden** (Kandidaten: SHT4x, SHT31, AHT20 — I2C, über Grove-Anschluss) |
| Stromversorgung | Aktuell Netzteil (3.3V oder 5V, Display unterstützt beides). Batteriebetrieb als späteres Ziel geplant. |

---

## 3. Pinbelegung

### Display (SPI) — Waveshare 7,5" HAT ↔ Xiao ESP32-S3

| HAT-Pin | Funktion | Xiao-Pin |
|---|---|---|
| VCC | 3.3V | 3V3 |
| GND | Masse | GND |
| DIN | SPI MOSI | D10 |
| CLK | SPI SCK | D8 |
| CS | Chip Select | D0 |
| DC | Data/Command | D1 |
| RST | Reset | D9 (frei, da MISO ungenutzt) |
| BUSY | Busy-Signal | D3 (setzt voraus, dass die Buzzer-Leiterbahn getrennt wird) bzw. letzter freier Pin |

Kein MISO nötig — Display wird nur beschrieben, sendet keine Daten zurück.

### Expansion Board — Gesamtbelegung

| Pin | Belegung |
|---|---|
| D0 | Display CS |
| D1 | Display DC |
| D2 | SD-Karte CS |
| D3 (A3) | Buzzer (per Leiterbahn-Schnitt abtrennbar; wird für Display-BUSY benötigt) |
| D4 | I2C SDA (GPIO5) |
| D5 | I2C SCL (GPIO6) |
| D6/D7 | UART TX/RX |
| D8 | SPI SCK |
| D9 | Display RST (SPI MISO ungenutzt) |
| D10 | SPI MOSI (Display DIN) |

### I2C-Bus (gemeinsam genutzt)

SDA = D4 (GPIO5), SCL = D5 (GPIO6) — Bus wird geteilt von:
- Onboard-OLED (Adresse 0x3C)
- Onboard-RTC PCF8563 (Adresse 0x51)
- externer Temperatur-/Feuchte-Sensor (Adresse je nach Modell, s. o.)

Keine Adresskonflikte, da I2C mehrere Geräte parallel erlaubt.

---

## 4. Software-Stack

- **Framework:** PlatformIO (nicht Arduino IDE) — klare Projektstruktur, Git-Versionierung
- **WLAN-Provisioning:** `WiFiManager` (tzapu) + `Preferences` (NVS) — siehe Abschnitt 5a
- **Display-Bibliothek:** GxEPD2
  - Display-Klasse: `GxEPD2_750_T7` (V2, 800×480) — **bestätigt** anhand des Panel-Aufdrucks `075BN-T7-D2 N2A4P05` auf der FPC-Kabelseite (siehe Abschnitt 2), nicht `GxEPD2_750c_Z8`
  - Framebuffer Full-Mode: 800×480/8 = 48.000 Byte RAM
  - Falls Xiao S3 **nicht** die „Sense"-Variante mit 8 MB PSRAM ist: **Paged-Mode** von GxEPD2 nutzen (nur Bildstreifen im RAM, mehrere Durchgänge) statt Full-Buffer
  - **Fast Partial Refresh** nutzen für häufig wechselnde Bereiche (Uhrzeit), **Full Refresh** selten für Ghosting-Reset (z. B. stündlich) und für seltener wechselnde Bereiche (Datum, KW, Sensorwerte)
- **Zeitquelle:** `time.h` (Standard ESP32/newlib), kein manuelles Berechnen nötig
  - Uhrzeit/Datum: `strftime()` mit `%H:%M`, `%d.%m.%Y` etc.
  - Wochentag: `tm.tm_wday` bzw. `strftime(%A)`
  - **ISO-Kalenderwoche:** `strftime()` mit `%V`
  - ISO-Jahr (wichtig an Jahresgrenzen): `strftime()` mit `%G`
  - ⚠️ `%V`/`%G` vor Verwendung auf der konkreten Arduino-Core/Toolchain-Version testen — nicht jede newlib-Version implementiert das fehlerfrei
- **NTP:**
  - Primär: `de.pool.ntp.org`
  - Fallback: `pool.ntp.org`
  - Zusätzlicher Fallback: `time.cloudflare.com`
  - Zeitzone: `configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "de.pool.ntp.org", "pool.ntp.org", "time.cloudflare.com");`
  - Sync-Häufigkeit: ca. 1×/Tag ausreichend (kein Stratum-1-Server nötig)
- **WLAN-Zugangsdaten:** `Preferences` (NVS) + `WiFiManager` (tzapu) — siehe Abschnitt 5a. **Kein** `secrets.h` mit hartcodierten Zugangsdaten mehr (siehe Hinweis unten in der Projektstruktur).

### Modulare Projektstruktur

```
src/
  main.cpp         — nur Orchestrierung, inkl. Sprachauswahl der Display-Strings
  display.cpp/h    — e-Paper Rendering
  net_time.cpp/h   — WLAN-Verbindung (inkl. AP-Fallback-Provisioning, siehe Abschnitt 5a) + NTP-Zeit
  rtc.cpp/h        — externe RTC PCF8563 (Zeit lesen/schreiben, Alarm-Platzhalter)
  sensor.cpp/h     — Temperatur-/Feuchte-Sensor, austauschbar über SENSOR_TYPE
  power.cpp/h      — Deep Sleep Management, WiFi-Toggle, RTC-Wakeup
lib/
include/
  config.h                 — zentrale Pin-/Konstanten-Definitionen, inkl. DISPLAY_LANGUAGE (siehe Abschnitt 1)
  <FontName><Size>pt7b.h   — selbst generierte Font-Header (siehe Abschnitt 6)
platformio.ini
```

⚠️ **Bewusst `net_time.cpp/h` statt `wifi.cpp/h`:** Windows-Dateisysteme sind
case-insensitiv. Ein lokales `wifi.h` kollidiert dort mit dem `#include <WiFi.h>`
der ESP32-Arduino-Core-Bibliothek, wodurch deren `WiFi`-Klasse nicht mehr gefunden
wird (Build-Fehler „'WiFi' was not declared in this scope"). Deshalb heißt das
WLAN-/NTP-Modul in diesem Projekt `net_time.cpp/h`.

⚠️ **Kein `secrets.h` mehr für WLAN-Zugangsdaten:** Ursprünglich war eine
gitignored `secrets.h` mit hartcodierten SSID/Passwort vorgesehen. Das wurde
verworfen zugunsten von `Preferences`(NVS) + `WiFiManager`, siehe Abschnitt 5a —
damit lassen sich Zugangsdaten ändern, ohne die Firmware neu zu bauen.

Grundprinzip: Bibliotheken per Namen referenzieren (z. B. „GxEPD2 v1.x"), nicht Library-Code einfügen — spart Tokens bei der Arbeit mit Claude Code.

---

## 5. Energie-/Power-Strategie

Ziel: Vorbereitung auf späteren Batteriebetrieb, auch wenn aktuell Netzbetrieb.

### Wakeup-Strategie
- **Nicht** den internen ESP32-Timer für Deep-Sleep-Wakeup nutzen (driftet stark)
- Stattdessen: **PCF8563**, INT-Pin an freien GPIO anschließen, ESP32 per `esp_sleep_enable_ext0_wakeup()` (oder ext1) darauf scharf schalten
- ESP32 verbraucht im Deep Sleep nur noch µA; Zeithaltung übernimmt komplett der PCF8563

**Timer-Register vs. Alarm-Register des PCF8563 — wichtige Unterscheidung:**

| | Timer-Register (Countdown) | Alarm-Register |
|---|---|---|
| Funktionsweise | 8-Bit-Countdown ab gesetztem Wert | Abgleich gegen Minute/Stunde/Tag/Wochentag |
| Taktquelle/Auflösung | wählbar: 4096 Hz, 64 Hz, **1 Hz**, 1/60 Hz — mit 1 Hz **sekundengenau** | **nur Minutengranularität**, keine Sekunden |
| Bezug | reines Intervall ab dem Setzen ("wecke in X Sekunden") | feste Uhrzeit ("wecke um HH:MM") |
| Einsatz in diesem Projekt | **primär genutzt** — siehe Abschnitt 5b (Tier-Strategie) | nicht benötigt, da die Timer-Lösung die Anforderungen sekundengenau abdeckt |

**Projektentscheidung:** Es wird ausschließlich das **Timer-Register mit 1-Hz-Quelle** verwendet
(nicht das Alarm-Register). Der PCF8563 liefert damit sowohl die Referenzzeit (normales
Uhrzeit-Register) als auch den sekundengenauen Countdown bis zum nächsten Wakeup — beides
läuft vom selben Quarz, daher keine Drift-Diskrepanz zwischen berechneter Sekundenanzahl und
tatsächlich verstrichener Zeit. Details zur Berechnung der Sleep-Dauer: siehe Abschnitt 5b.

### RTC-Genauigkeit / Sync-Intervall
Faustformel: Drift (s/Tag) = ppm-Wert × 86400 / 1.000.000

| Genauigkeit RTC | Drift/Tag | Sync nötig für max. 60 s Fehler |
|---|---|---|
| einfacher 32k-Quarz unkompensiert (~20 ppm) | ~1,7 s | alle ~35 Tage |
| Standard-Quarz mit Temp.-Schwankung (~50 ppm) | ~4,3 s | alle ~14 Tage |
| günstiger interner RC unkalibriert | mehrere s bis min/Tag | täglich oder öfter |
| TCXO (~2 ppm) | ~0,17 s | alle ~1 Jahr |

PCF8563 typische Drift: 5–20 ppm — Sync-Intervall entsprechend danach berechnen.

### WiFi zur Laufzeit ein/aus
Ablauf:
```
1. esp_netif_init() + esp_wifi_init() nur beim Sync-Zyklus aufrufen
2. esp_wifi_start() → verbinden → NTP holen → PCF8563 stellen
3. esp_wifi_stop() + esp_wifi_deinit() + esp_netif_deinit()
4. Danach normaler Deep Sleep bis zum nächsten PCF8563-Timer-Wakeup (siehe Abschnitt 5b)
```

### Wird für dieses Projekt benötigt
- I2C-Treiber (RTC, Sensor, ggf. OLED — gemeinsamer Bus)
- SPI-Treiber (e-Paper-Display, ggf. SD-Karte)
- WiFi-Treiber mit Laufzeit-Init/Deinit (nur für NTP-Sync)
- Deep-Sleep + Power-Management (`CONFIG_PM_ENABLE`)
- GPIO-Interrupt/ext-Wakeup (PCF8563-INT-Pin)
- NVS (Flash-Storage), falls Einstellungen/letzter Sync-Zeitpunkt persistiert werden sollen

### Wird NICHT benötigt — im Build deaktivieren
- `CONFIG_BT_ENABLED=n` — Bluetooth komplett aus
- interner ESP32-RTC-Timer für Zeithaltung (ersetzt durch PCF8563)
- ADC-Kalibrierung/Touch/CAN/DAC
- HTTP-Server, mDNS, SNTP-Dauerbetrieb (nur kurzer NTP-Client-Call im Sync-Fenster)
- Buzzer-Treiber (PWM/LEDC) — nur relevant, falls Buzzer-Leiterbahn nicht getrennt wird
- USB-CDC/JTAG im Dauerbetrieb — für Produktivbuild nach Debugging deaktivieren

---

## 5a. WLAN-Zugangsdaten — Speicherung & Änderung ohne Neuflash

Ziel: SSID/Passwort sollen änderbar sein, **ohne die Firmware neu zu bauen und zu flashen**
(z. B. bei Routerwechsel oder neuem Passwort).

### Speicherung
- Bibliothek: **`Preferences`** (nutzt intern NVS — Non-Volatile Storage, ein eigener Flash-Bereich)
- **Kein** Hartcodieren von SSID/Passwort im Quellcode (kein `secrets.h` mehr, siehe Abschnitt 4)
- Zugangsdaten überleben ein Neuflashen der Firmware, solange die NVS-Partition dabei nicht mitgelöscht wird

```cpp
#include <Preferences.h>
Preferences prefs;

void saveWifi(String ssid, String pass) {
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
}

void loadWifi(String &ssid, String &pass) {
  prefs.begin("wifi", true);
  ssid = prefs.getString("ssid", "");
  pass = prefs.getString("pass", "");
  prefs.end();
}
```

### Ändern der Zugangsdaten: eigener AP mit Captive Portal (Kernstrategie)
Bibliothek: **`WiFiManager`** (tzapu) — Standardansatz für genau dieses Szenario.

Ablauf:
1. ESP32 versucht beim Boot, sich mit den in NVS gespeicherten Zugangsdaten zu verbinden
2. Schlägt das fehl (falsches/geändertes Passwort, Router nicht erreichbar) **oder** wird ein
   Reset-/Config-Trigger ausgelöst → ESP32 öffnet einen **eigenen Access Point** mit
   Captive-Portal-Konfigurationsseite
3. Verbindung per Smartphone/Laptop mit diesem AP → Eingabe der neuen SSID/Passwort über die
   automatisch angezeigte Weboberfläche
4. `WiFiManager` speichert die neuen Daten selbstständig (intern ebenfalls über NVS) und
   verbindet sich automatisch neu

Minimalbeispiel:
```cpp
#include <WiFiManager.h>
WiFiManager wm;
wm.autoConnect("ESP32-Setup");   // AP-Name im Fallback-Fall
```

**Trigger für den Config-Modus:** noch festzulegen (siehe TODO), z. B.:
- automatisch bei fehlgeschlagener Verbindung (Standardverhalten von `autoConnect()`)
- zusätzlich manuell per Taster/Boot-Pin-Kombination erzwingbar, um den Config-Modus auch bei
  funktionierender bestehender Verbindung gezielt aufzurufen (z. B. für Umzug in ein neues WLAN)

### Verworfene/Zusatz-Option: WPS über die Fritzbox
- Technisch möglich über `esp_wps.h` (WPS-PBC-Modus, Taste an der Fritzbox drücken)
- **Nicht** als Hauptlösung gewählt: erfordert physischen Zugriff auf den Router bei jedem
  Setup, und nicht jede Fritzbox-Firmware hat WPS in jedem Modus aktiviert
- Bleibt als mögliche Komfort-Ergänzung im Hinterkopf, aber `WiFiManager` ist der primäre Weg

### Zusammenspiel mit der Energie-/Deep-Sleep-Strategie (Abschnitt 5)
- Der reguläre Betrieb (WiFi nur kurz für NTP-Sync aktivieren, siehe „WiFi zur Laufzeit ein/aus"
  oben) bleibt unverändert
- Der AP-Fallback-Modus ist ein **Sonderfall**, der nur bei fehlgeschlagener Verbindung oder
  explizitem Trigger aktiv wird — in diesem Modus bleibt WiFi (als AP) aktiv, bis die
  Konfiguration abgeschlossen ist; kein Deep Sleep während des Config-Modus

### Default-Zugangsdaten beim Firmware-Upload vorschreiben (optional)
Ziel: Erstinbetriebnahme ganz ohne Captive-Portal-Schritt ermöglichen, ohne vom NVS/WiFiManager-
Ansatz abzuweichen.

- In `platformio.ini` per `build_flags` einen Default hinterlegen, z. B.
  `-DDEFAULT_WIFI_SSID=\"...\" -DDEFAULT_WIFI_PASS=\"...\"` — Werte kommen aus einer lokalen,
  **gitignoreten** Datei (z. B. `wifi_defaults.ini`, analog zum verworfenen `secrets.h`-Prinzip,
  hier aber nur als einmaliger Startwert, nicht als laufende Quelle)
- Beim allerersten Boot prüfen: ist in `Preferences`/NVS bereits eine SSID gespeichert?
  - **Nein** → Default-Werte aus den Build-Flags einmalig per `saveWifi()` in NVS schreiben
  - **Ja** → Build-Flags ignorieren, ganz normal mit den gespeicherten Werten weiterarbeiten
- Ab dem zweiten Boot läuft alles ausschließlich über NVS; `WiFiManager` greift bei jedem
  weiteren Boot nur noch auf die (ggf. inzwischen per AP-Fallback geänderten) NVS-Werte zu
- Alternative (aufwendiger, hier nicht gewählt): fertiges NVS-Partition-Image per
  `nvs_partition_gen.py` vorab erzeugen und getrennt vom Firmware-Upload per `esptool.py`
  flashen

---

## 5b. Deep-Sleep-Tier-Strategie (Kernlogik für Wakeup-Timing)

Grundprinzip: **Es gibt nur einen Wach-Rhythmus (jede Minute)** — Stunden- und Tageswechsel
sind lediglich Minutenwechsel, bei denen zusätzlich etwas passiert. Kein Polling; nach jeder
Aktion wird exakt berechnet, wann und wie lange vorher der nächste Wakeup sein muss, dann
Deep Sleep bis dahin über das PCF8563-**Timer-Register mit 1-Hz-Quelle** (siehe Abschnitt 5,
Wakeup-Strategie).

### Ablauf pro Zyklus

**1. Aufwachen → RTC-Zeit lesen**
Aktuelle Zeit vom PCF8563 auslesen (der ESP32 hat im Deep Sleep keine eigene laufende Uhr).
Gleichzeitig Boot-Check: weicht die gelesene Zeit stark vom erwarteten Zieltarget ab (z. B.
durch Stromausfall) → sofort Full-Refresh erzwingen statt normaler Minuten-Logik.

**2. Aktions-Tier bestimmen**
Da beim letzten Einschlafen bereits gezielt auf diesen Minutenwechsel gewartet wurde, ist
deterministisch bekannt, was ansteht:
- `minute == 0 && hour == 0` → **Mitternacht-Tier**: WiFi an, NTP-Sync, RTC stellen,
  Full-Refresh, WiFi aus
- `minute == 0` (sonst) → **Stunden-Tier**: kompletter Uhr-Bereich partiell refreshen +
  Temp/Feuchte-Check
- sonst → **Minuten-Tier**: nur Minutenbereich partiell refreshen + Temp/Feuchte-Check
- **zusätzlich, unabhängig vom Tier:** Ghosting-Zähler prüfen — Schwellwert erreicht? →
  Full-Refresh einschieben (auch außerhalb der Mitternacht), siehe Abschnitt 7

**3. Aktion(en) ausführen**

**4. Nächstes Ziel berechnen**
`next_target = aktuelles_target + 60s` (der nächste Minutenwechsel)

**5. Budget für das nächste Ziel ermitteln**
Tier von `next_target` vorausschauend bestimmen (gleiche Logik wie Schritt 2) und eine
geschätzte Vorlaufzeit zuordnen:

| Tier | Geschätztes Budget | Warum |
|---|---|---|
| Minute | z. B. 1–2 s | nur Partial Refresh, kein WiFi |
| Stunde | z. B. 2–3 s | etwas größerer Partial Refresh |
| Mitternacht | z. B. 8–15 s | WiFi-Connect + NTP-Roundtrip sind die variable Größe |

Plus fixer Sockel für ESP32-Bootoverhead nach Deep-Sleep-Wakeup (Peripherie-/I2C-/SPI-Init,
üblicherweise wenige hundert ms) als Konstante einrechnen.

**6. Timer setzen und schlafen**
`sleep_seconds = (next_target − budget[next_tier]) − aktuelle_RTC_zeit`
→ PCF8563-Timer-Register (1-Hz-Quelle) auf `sleep_seconds` setzen, Deep Sleep.

### Zusatzpunkte

- **Budgets nicht raten, sondern messen:** Beim ersten produktiven Lauf tatsächliche Dauer
  jeder Aktion stoppen (`millis()` vor/nach Refresh bzw. WiFi-Connect) und mit Sicherheitsmarge
  (z. B. +30–50 %) als Budget übernehmen. Optional in NVS persistieren und über die Zeit
  kalibrieren (z. B. gleitender Mittelwert der letzten X Mitternachts-Zyklen) — WiFi-Connect-
  Dauer schwankt am meisten, dafür lohnt sich eine großzügige Marge am ehesten.
- **Negative/zu knappe Sleep-Zeit abfangen:** Falls eine Aktion länger dauert als das Budget
  vorsah (`sleep_seconds` würde negativ oder sehr klein) → nicht crashen, sondern minimalen
  Sleep (z. B. 1 s) einplanen und den nächsten Zyklus normal nachziehen lassen (Anzeige dann
  mal 1–2 s zu spät, aber nichts hängt)
- **Kein Drift-Problem:** Referenzzeit und Countdown-Timer laufen vom selben PCF8563-Quarz —
  keine Rundungs-/Drift-Diskrepanz zwischen berechneter Sekundenanzahl und tatsächlich
  verstrichener Zeit
- **NTP-Timeout bleibt im Mitternacht-Tier eingebettet:** der bereits definierte NTP-Fallback
  (Timeout + Weiterarbeiten mit letzter RTC-Zeit, siehe Abschnitt 7) sitzt innerhalb des
  Mitternacht-Aktionsblocks und beeinflusst nur, ob die RTC neu gestellt wird — nicht die
  grundsätzliche Wach-Logik

---

## 6. Font-Strategie

**Format:** Adafruit-GFX-Font-Format (`.h`-Header), TTF → `.h` via `fontconvert`-Tool aus dem Adafruit-GFX-Library-Repo.

### Wichtige technische Fakten
- Fonts müssen **direkt in der Zielpixelgröße** konvertiert werden — kein nachträgliches Hochskalieren (führt zu Treppenstufen/Unschärfe)
- Pro benötigter Schriftgröße muss `fontconvert` **separat** aufgerufen werden
- **Harte Grenze:** `GFXglyph`-Struct speichert Glyph-Höhe/-Breite als `uint8_t` → **max. 255 px** pro Zeichen. `xOffset`/`yOffset` als `int8_t` (−128 bis 127) können bei sehr ausladenden Zeichen schon vorher zu Clipping führen. Praktisch sicherer Bereich: bis ca. 200–220 px.
- Pixel-Doppelung (Verpixelung) tritt **nur** bei `setTextSize() > 1` auf, nicht automatisch ab einer bestimmten Größe
- Vorgefertigte Adafruit-Font-Header (`Fonts/FreeSansBold24pt7b.h` etc.) sind **nicht nutzbar**: zu wenige Punktgrößen, keine Monospace-Option
- Fonts landen im Flash (PROGMEM), nicht im RAM — mehrere Font-Größen sind unkritisch für RAM-Verbrauch

### Layout-Anforderung: Uhrzeit-Ziffern
- Ursprüngliche Schätzung „ca. 267 px" für „12:05" war zu hoch — anhand der
  tatsächlichen Layout-Vorlage (`docs/Layout.png`, pixelgenau vermessen,
  siehe `docs/layout.md`) sind es **187 px**, damit unproblematisch unter
  der 255px-`uint8_t`-Grenze (Option 1 aus den ursprünglich erwogenen
  Alternativen — Siebensegment/Bitmaps sind damit hinfällig).
- Trotzdem trat ein Sonderfall auf: `fontconvert`s `yAdvance`-Feld
  (Zeilenabstand, skaliert mit der Zielgröße, nicht mit der reinen
  Glyphen-Höhe) überschreitet auch bei 187px noch den `uint8_t`-Bereich →
  `tools/fonts.py`s `clamp-yadvance`-Subcommand klemmt das automatisch
  (siehe `include/fonts/README.md`/`tools/README.md`; `yAdvance` wird von
  diesem Projekt ohnehin nicht genutzt, da kein mehrzeiliger Text vorkommt).

### Benötigte Font-Größen (final, pixelgenau aus `docs/Layout.png` vermessen)
| Font | Zielgröße | Verwendung | Zeichensatz |
|---|---|---|---|
| `FONT_TIME` | 187 px | Uhrzeit (`12:05`) | `0123456789:` |
| `FONT_SMALL` | 38 px | Kalenderwoche (`KW32`) + Wochentag (`Donnerstag`) | volles ASCII (32–126) |
| `FONT_MEDIUM` | 53 px | Datum + Temperatur + Luftfeuchte | ASCII + Latin-1 (32–176, inkl. `°`) |

Effektiv 3 Fonts, aber anders gruppiert als ursprünglich angenommen: nicht
"Buchstaben+Zahlen" vs. "reine Ziffern", sondern nach tatsächlich
gemessener Pixelgröße in der Layout-Vorlage — Datum landet bei derselben
Größe wie die Sensorwerte, nicht bei Kalenderwoche/Wochentag. Details und
die Messmethode (pixelgenau, ohne Verfälschung durch Ober-/Unterlängen und
das Gradzeichen) siehe `docs/layout.md`.

### Schriftart
- **Final entschieden: Droid Sans Mono.** Kein Bold-Schnitt der Schriftart
  verfügbar → synthetischer Bold-Schnitt per FontForge erzeugt
  (`tools/fonts.py`s `make-bold`-Subcommand, siehe `tools/README.md`).
- Ursprünglich gewünscht: **Monospace821 BT** — proprietärer Bitstream-Font, Lizenzlage für dieses Projekt unklar/uneinheitlich angegeben (Personal-Use-Angaben widersprüchlich)
- Erwogene freie Alternativen (OFL-lizenziert, für jeden Zweck frei nutzbar): Space Mono, JetBrains Mono, Courier Prime, IBM Plex Mono — letztlich Droid Sans Mono gewählt

### Konvertierungs-Workflow
Tatsächlich umgesetzt in `tools/generate_fonts.sh` (ruft `tools/fonts.py`-
Subcommands auf, siehe `tools/README.md` für die vollständige Anleitung),
nicht als manuelle `fontconvert`-Einzelaufrufe. Kurzfassung:
```bash
git clone https://github.com/adafruit/Adafruit-GFX-Library
cd Adafruit-GFX-Library/fontconvert
make    # benötigt freetype

cd tools/
./generate_fonts.sh   # erzeugt alle Font-Header aus include/fonts/README.md in einem Rutsch
```

Online-Alternative ohne lokalen Build: GFX-Font-Generator im Web (Suchbegriff „adafruit gfx font converter online").

---

## 7. Display-Refresh-Strategie

### Grundzyklus (Tiers)
Die eigentliche Ausführlogik — Details zur Wakeup-Berechnung dazu in Abschnitt 5b:
- **jede Minute:** Minutenbereich partiell refreshen; Temperatur/Feuchte werden nur bei
  Änderung des **angezeigten** (gerundeten) Werts mit-refresht
- **jede Stunde:** gesamter Uhr-Bereich partiell refreshen (inkl. Temp/Feuchte-Check)
- **einmal täglich, um Mitternacht:** kompletter Full-Refresh; WLAN kurz an, NTP-Sync, RTC
  stellen, Refresh exakt um 0:00, danach WLAN wieder aus

### Offene Punkte für die Code-Generierung
Diese Punkte wurden als "beim Programmieren unbedingt berücksichtigen" markiert:

- **Ghosting-Zähler:** Anzahl Partial Refreshes seit letztem Full Refresh mitzählen, nach Schwellwert automatisch Full Refresh erzwingen
- **Rundung/Hysterese bei Sensorwerten:** verhindert ständiges Neuzeichnen durch Sensor-Jitter (z. B. Temperatur pendelt zwischen 24,2/24,3°C)
- **NTP-Fallback:** Verhalten definieren, falls kein NTP-Server erreichbar ist (z. B. letzte bekannte PCF8563-Zeit weiterverwenden)
- **Boot-Check für verpasste Refreshes:** beim Aufwachen prüfen, ob ein erwarteter Refresh-Zyklus verpasst wurde (z. B. durch Stromausfall) und ggf. nachholen
- **Ziffernbreite bei Partial Refresh:** Bei proportionalen Schriftarten kann eine schmalere Ziffer eine breitere nicht vollständig überschreiben (Bildreste bleiben sichtbar). Lösung: feste Zeichenbreite (Monospace hilft hier direkt) oder kompletten Ziffern-Bounding-Box-Bereich löschen vor Neuzeichnen
- **Sommer-/Winterzeit-Umstellung:** Die DST-Umstellung fällt nicht auf Mitternacht, sondern meist auf 2 bzw. 3 Uhr früh. Falls die Stunden-Tier-Logik zeitbasiert zählt statt einfach die aktuelle RTC-Zeit auszulesen, kann es bei der Umstellung zu einer übersprungenen oder doppelten Stunde kommen — Stunden-Tier-Erkennung muss robust gegen diesen Sprung sein (z. B. immer `hour`/`minute` direkt aus der gelesenen RTC-Zeit ableiten, nicht mitzählen)
- Deep-Sleep mit RTC-Timer-Interrupt statt aktivem Polling nutzen (energieeffizienter) — siehe Abschnitt 5b für die konkrete Umsetzung

---

## 8. Entwicklungsumgebung

- **Editor:** VS Code (offizielle Microsoft-Version empfohlen wegen vollem Marketplace-Zugriff, insbesondere für PlatformIO/C++/Python-Debugging — Alternative VSCodium ohne Microsoft-Telemetrie, aber eingeschränkter Marketplace via Open VSX)
- **Build-System:** PlatformIO-Extension in VS Code
- **Versionskontrolle:** lokales Git-Repository, damit Claude Code direkt auf den Projektordner zugreifen kann (kein Copy-Paste von Code nötig, spart Tokens)
- **Claude Code:** über npm installiert + VS-Code-Extension, direkter Zugriff auf Projektdateien

### Git-Setup (Kurzreferenz)
```bash
cd /pfad/zu/deinem/projekt
git init
# .gitignore anlegen (build/, .pio/, .env etc.)
git add .
git commit -m "Initial commit"
# optional:
git remote add origin <repo-url>
git push -u origin main
```

---

## 9. Mechanik / Gehäuse (falls relevant)

- **Waveshare 7,5" Display (13504):** keine offizielle STEP-Datei, aber Maßzeichnung als PDF im Handbuch: https://files.waveshare.com/wiki/7.5inch_e-Paper_(H)/7.5inch_e-Paper_(H).pdf (Kapitel 4 „Mechanical Drawing of EPD Module")
- **Seeed XIAO ESP32-S3:** offizielle STEP/DXF-Dateien via Seeed-Wiki: https://wiki.seeedstudio.com/SeeedStudio_XIAO_Series_Introduction/
- **XIAO Expansion Board:** ebenfalls STEP/DXF auf derselben Wiki-Seite verfügbar (falls Sense-Variante)
- Referenz/Ausgangspunkt: vorhandenes Waveshare-Schutzgehäuse WS16089 für die reine Display-Platine

---

## 10. Offene Punkte / TODOs

- [x] Finale Schriftart festgelegt: Droid Sans Mono (synthetischer Bold-Schnitt, siehe Abschnitt 6 / `tools/README.md`)
- [x] Lösung für Uhrzeit-Ziffern >255px festgelegt: Option 1, tatsächlich nur 187 px (pixelgenau aus `docs/Layout.png` vermessen, siehe Abschnitt 6/`docs/layout.md`)
- [ ] Finalen I2C-Sensor für Temp/Feuchte auswählen (SHT4x / SHT31 / AHT20 / anderer) — Code liegt bereit (`SENSOR_TYPE` in `config.h`), aktuell `SENSOR_TYPE_NONE`-Platzhalter mit Dummy-Werten
- [x] Panel-Aufdruck auf FPC-Kabelseite geprüft: `075BN-T7-D2 N2A4P05` → `GxEPD2_750_T7` bestätigt (siehe Abschnitt 2/4)
- [ ] Prüfen, ob Xiao S3 „Sense"-Variante mit 8 MB PSRAM vorliegt (bestimmt Full-Buffer vs. Paged-Mode)
- [x] Git-Repository eingerichtet
- [x] Layout-Skizze als Referenzdatei ins Projekt aufgenommen: `docs/Layout.png` (Mockup) + pixelgenau vermessene Koordinaten in `docs/layout.md`/`src/display.cpp`; Fonts mit den neuen Zielgrößen (187/38/53px) regeneriert, Variablennamen gegen echten `fontconvert`-Output verifiziert — Verifikation am echten Display steht noch aus
- [ ] Batteriebetrieb — Architektur ist umgesetzt (PCF8563-Timer-Register-Wakeup, Deep-Sleep-Tier-Zyklus in `main.cpp`, siehe Abschnitt 5/5b), aber noch inaktiv/ungetestet: `DEEP_SLEEP_ENABLED` steht auf 0 (Netzbetrieb) und `PIN_RTC_INT` ist noch Platzhalter (-1) in `config.h`
- [ ] Trigger für WLAN-Config-Modus (AP-Fallback, siehe Abschnitt 5a) festlegen: aktuell nur automatisch bei Verbindungsfehler (`wifi_connect_or_configure()`); zusätzlicher manueller Taster/Boot-Pin-Trigger noch nicht umgesetzt
- [x] AP-Name/Passwort für den `WiFiManager`-Konfigurations-AP festgelegt: `WIFI_CONFIG_AP_NAME` in `config.h` (`Wall_Clock_Setup`); `WIFI_CONFIG_AP_PASSWORD` bewusst **nicht** in `config.h` (das ist eine committete Datei) — Fallback dort ist `""` (offener AP), das echte Passwort gehört wie die WLAN-Zugangsdaten ins gitignorete `wifi_defaults.ini` (siehe `wifi_defaults.ini.example`), min. 8 Zeichen sonst bleibt der AP offen
- [ ] Konkrete Budget-Werte (Sekunden pro Tier) für die Deep-Sleep-Berechnung aus Abschnitt 5b anhand realer Messungen festlegen — aktuell grobe Platzhalter (`TIER_BUDGET_MINUTE_S`/`_HOUR_S`/`_MIDNIGHT_S` in `config.h`)
- [x] Default-WLAN-Zugangsdaten per Build-Flags werden genutzt: `wifi_defaults.ini` (lokal, gitignored; Vorlage: `wifi_defaults.ini.example`)

---

## 11. Arbeitsweise mit Claude Code (Präferenzen)

- Modularer Aufbau bevorzugt (siehe Struktur in Abschnitt 4), damit gezielt an einzelnen Dateien gearbeitet werden kann, ohne das ganze Projekt einzufügen
- Bibliotheken per Namen/Version referenzieren statt Code einzufügen
- Pinbelegung und Architekturentscheidungen (diese Datei) als Kontext nutzen, nicht erneut erfragen
- **Quellcode-Kommentare, Bezeichner und Log-/Serial-Debug-Ausgaben werden auf Englisch verfasst** — unabhängig von der Sprache der Display-Anzeige, die standardmäßig Deutsch bleibt (siehe Abschnitt 1, `DISPLAY_LANGUAGE`). Diese Datei (CLAUDE.md) selbst bleibt davon ausgenommen und weiterhin auf Deutsch.
