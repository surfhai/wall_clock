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
| Display | Waveshare 7,5" e-Paper HAT, schwarz/weiß, **Artikelnummer 013504**, **V2-Variante (800×480, seit Sept. 2023)**, 170° Sichtwinkel, direkte SPI-Ansteuerung (**keine** IT8951-Variante) |
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
- **Display-Bibliothek:** GxEPD2
  - Display-Klasse: `GxEPD2_750_T7` (V2, 800×480) — **beim Einrichten anhand des Panel-Aufdrucks auf der FPC-Kabelseite verifizieren**, alternativ `GxEPD2_750c_Z8`
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

### Modulare Projektstruktur

```
src/
  main.cpp         — nur Orchestrierung, inkl. Sprachauswahl der Display-Strings
  display.cpp/h    — e-Paper Rendering
  net_time.cpp/h   — WLAN-Verbindung + NTP-Zeit
  rtc.cpp/h        — externe RTC PCF8563 (Zeit lesen/schreiben, Alarm-Platzhalter)
  sensor.cpp/h     — Temperatur-/Feuchte-Sensor, austauschbar über SENSOR_TYPE
  power.cpp/h      — Deep Sleep Management, WiFi-Toggle, RTC-Wakeup
lib/
include/
  config.h                 — zentrale Pin-/Konstanten-Definitionen, inkl. DISPLAY_LANGUAGE (siehe Abschnitt 1)
  secrets.h(.example)      — WLAN-Zugangsdaten (secrets.h ist gitignored)
  <FontName><Size>pt7b.h   — selbst generierte Font-Header (siehe Abschnitt 6)
platformio.ini
```

⚠️ **Bewusst `net_time.cpp/h` statt `wifi.cpp/h`:** Windows-Dateisysteme sind
case-insensitiv. Ein lokales `wifi.h` kollidiert dort mit dem `#include <WiFi.h>`
der ESP32-Arduino-Core-Bibliothek, wodurch deren `WiFi`-Klasse nicht mehr gefunden
wird (Build-Fehler „'WiFi' was not declared in this scope"). Deshalb heißt das
WLAN-/NTP-Modul in diesem Projekt `net_time.cpp/h`.

Grundprinzip: Bibliotheken per Namen referenzieren (z. B. „GxEPD2 v1.x"), nicht Library-Code einfügen — spart Tokens bei der Arbeit mit Claude Code.

---

## 5. Energie-/Power-Strategie

Ziel: Vorbereitung auf späteren Batteriebetrieb, auch wenn aktuell Netzbetrieb.

### Wakeup-Strategie
- **Nicht** den internen ESP32-Timer für Deep-Sleep-Wakeup nutzen (driftet stark)
- Stattdessen: **PCF8563-Alarm/Timer** setzen, INT-Pin des PCF8563 an freien GPIO anschließen, ESP32 per `esp_sleep_enable_ext0_wakeup()` (oder ext1) darauf scharf schalten
- ESP32 verbraucht im Deep Sleep nur noch µA; Zeithaltung übernimmt komplett der PCF8563

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
4. Danach normaler Deep Sleep bis zum nächsten PCF8563-Alarm
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
- Zielgröße Uhrzeit „12:05" liegt bei ca. **267 px** Ziffernhöhe → **überschreitet die 255px-uint8_t-Grenze**
- Optionen bei Bedarf:
  1. Knapp unter 255 px bleiben (z. B. 250 px)
  2. Siebensegment-Stil selbst aus Rechtecken/Polygonen zeichnen (kein Font-Speicher nötig, beliebig scharf)
  3. Vorgerenderte Bitmaps pro Ziffer via `drawBitmap()`
- **Entscheidung dazu noch offen** — muss vor Font-Konvertierung final getroffen werden

### Benötigte Font-Größen (mind. 3, je nach Layout)
| Verwendung | Zeichensatz |
|---|---|
| Große Font (Uhrzeit 12:05) | `0123456789:` |
| Mittlere Font (Datum) | `0123456789-` |
| Mittlere Font (Temp/Feuchte) | `0123456789,%°CrLF` |
| Kleine/mittlere Font (KW + Wochentag) | `A-Z`, Umlaute (Ä/Ö/Ü), `0-9` |

Empfehlung: gemeinsame Buchstaben+Zahlen-Font für Wochentag/KW, reine Ziffern-Font (+Sonderzeichen) für Uhrzeit/Datum/Sensorwerte → effektiv 3 Fonts statt 4.

### Schriftart
- **Noch nicht final entschieden.**
- Ursprünglich gewünscht: **Monospace821 BT** — proprietärer Bitstream-Font, Lizenzlage für dieses Projekt unklar/uneinheitlich angegeben (Personal-Use-Angaben widersprüchlich)
- Freie Alternativen (OFL-lizenziert, für jeden Zweck frei nutzbar): **Space Mono**, **JetBrains Mono**, **Courier Prime**, **IBM Plex Mono**
- Wichtig: bei der Wahl echte Monospace-TTF verwenden (nicht nur optisch ähnlich)

### Konvertierungs-Workflow
```bash
git clone https://github.com/adafruit/Adafruit-GFX-Library
cd Adafruit-GFX-Library/fontconvert
make    # benötigt freetype

# Pro Zielgröße + Zeichenbereich (ASCII start/end optional):
./fontconvert <font>.ttf <pixelgröße> <ascii_start> <ascii_end> > include/<Name><Größe>.h

# Beispiele:
./fontconvert SpaceMono-Bold.ttf 250 48 58  > include/MonoClock250.h    # Ziffern+Doppelpunkt
./fontconvert SpaceMono-Bold.ttf 60  32 126 > include/MonoDate60.h      # Datum/Wochentag
./fontconvert SpaceMono-Bold.ttf 50  32 126 > include/MonoSensor50.h    # Temp/Feuchte
```
Empfehlung: Shell-Skript/Makefile anlegen, das alle benötigten Font/Größe-Kombinationen in einem Rutsch erzeugt — nur bei Font-/Layoutänderung neu ausführen.

Online-Alternative ohne lokalen Build: GFX-Font-Generator im Web (Suchbegriff „adafruit gfx font converter online").

---

## 7. Display-Refresh-Strategie — offene Punkte für die Code-Generierung

Diese Punkte wurden als "beim Programmieren unbedingt berücksichtigen" markiert:

- **Ghosting-Zähler:** Anzahl Partial Refreshes seit letztem Full Refresh mitzählen, nach Schwellwert automatisch Full Refresh erzwingen
- **Rundung/Hysterese bei Sensorwerten:** verhindert ständiges Neuzeichnen durch Sensor-Jitter (z. B. Temperatur pendelt zwischen 24,2/24,3°C)
- **NTP-Fallback:** Verhalten definieren, falls kein NTP-Server erreichbar ist (z. B. letzte bekannte PCF8563-Zeit weiterverwenden)
- **Boot-Check für verpasste Refreshes:** beim Aufwachen prüfen, ob ein erwarteter Refresh-Zyklus verpasst wurde (z. B. durch Stromausfall) und ggf. nachholen
- **Ziffernbreite bei Partial Refresh:** Bei proportionalen Schriftarten kann eine schmalere Ziffer eine breitere nicht vollständig überschreiben (Bildreste bleiben sichtbar). Lösung: feste Zeichenbreite (Monospace hilft hier direkt) oder kompletten Ziffern-Bounding-Box-Bereich löschen vor Neuzeichnen
- Deep-Sleep mit RTC-Alarm-Interrupt statt aktivem Polling nutzen (energieeffizienter)

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

- [ ] Finale Schriftart festlegen (Space Mono / JetBrains Mono / Courier Prime / andere)
- [ ] Lösung für Uhrzeit-Ziffern >255px festlegen (kleiner bleiben / Siebensegment / Bitmaps)
- [ ] Finalen I2C-Sensor für Temp/Feuchte auswählen (SHT4x / SHT31 / AHT20 / anderer)
- [ ] Panel-Aufdruck auf FPC-Kabelseite prüfen zur endgültigen Bestätigung `GxEPD2_750_T7` vs. `GxEPD2_750c_Z8`
- [ ] Prüfen, ob Xiao S3 „Sense"-Variante mit 8 MB PSRAM vorliegt (bestimmt Full-Buffer vs. Paged-Mode)
- [ ] Git-Repository anlegen (bisher noch nicht eingerichtet)
- [ ] Layout-Skizze (Positionierung der Elemente auf 800×480) noch als Referenzdatei ins Projekt aufnehmen
- [ ] Batteriebetrieb — aktuell nur konzeptionell vorbereitet, nicht umgesetzt (Netzbetrieb aktiv)

---

## 11. Arbeitsweise mit Claude Code (Präferenzen)

- Modularer Aufbau bevorzugt (siehe Struktur in Abschnitt 4), damit gezielt an einzelnen Dateien gearbeitet werden kann, ohne das ganze Projekt einzufügen
- Bibliotheken per Namen/Version referenzieren statt Code einzufügen
- Pinbelegung und Architekturentscheidungen (diese Datei) als Kontext nutzen, nicht erneut erfragen
- **Quellcode-Kommentare, Bezeichner und Log-/Serial-Debug-Ausgaben werden auf Englisch verfasst** — unabhängig von der Sprache der Display-Anzeige, die standardmäßig Deutsch bleibt (siehe Abschnitt 1, `DISPLAY_LANGUAGE`). Diese Datei (CLAUDE.md) selbst bleibt davon ausgenommen und weiterhin auf Deutsch.
