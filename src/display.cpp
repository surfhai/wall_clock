#include "display.h"
#include "config.h"

#include <SPI.h>
// GxEPD2_BW.h already conditionally includes all panel drivers (including
// epd/GxEPD2_750_T7.h) - no separate include needed.
// TODO: verify the panel print on the FPC cable side and switch to
// GxEPD2_750c_Z8 if needed (see CLAUDE.md section 4/10).
#include <GxEPD2_BW.h>

// TODO (font strategy, CLAUDE.md section 6): these are just placeholder
// fonts from the standard Adafruit GFX library. Once the final typeface
// (Space Mono / JetBrains Mono / ...) and size are decided, replace with
// the self-generated headers from include/fonts/ (see
// include/fonts/README.md).
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>

#define FONT_TIME     FreeSansBold24pt7b
#define FONT_INFO     FreeSans12pt7b
#define FONT_SENSOR   FreeSans9pt7b

// Full-buffer mode (needs PSRAM) vs. paged mode, see CLAUDE.md section 4
// and config.h/platformio.ini (BOARD_HAS_PSRAM).
#if defined(BOARD_HAS_PSRAM)
using EPaperDisplay = GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT>;
#else
using EPaperDisplay = GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT / 4>;
#endif

static EPaperDisplay display(GxEPD2_750_T7(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));

static uint16_t s_ghostingCounter = 0;

// ---------------------------------------------------------------------------
// Layout — placeholder until the final layout sketch is available
// (see CLAUDE.md section 10). All regions refer to 800x480.
// ---------------------------------------------------------------------------
static const int16_t TIME_X = 40,  TIME_Y = 30,  TIME_W = 500, TIME_H = 160;
static const int16_t INFO_X = 40,  INFO_Y = 200, INFO_W = 720, INFO_H = 140;
static const int16_t SENSOR_X = 40, SENSOR_Y = 360, SENSOR_W = 720, SENSOR_H = 100;

static void drawText(const char *text, int16_t x, int16_t y, const GFXfont *font) {
    display.setFont(font);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(x, y);
    display.print(text);
}

static void drawAllFields(const DisplayData &data) {
    // Time — large field
    drawText(data.timeStr, TIME_X, TIME_Y + 100, &FONT_TIME);

    // Weekday, date, calendar week
    char infoLine[48];
    snprintf(infoLine, sizeof(infoLine), "%s, %s  %s",
             data.weekdayStr, data.dateStr, data.weekStr);
    drawText(infoLine, INFO_X, INFO_Y + 30, &FONT_INFO);

    // Temperature / humidity
    char sensorLine[24];
    snprintf(sensorLine, sizeof(sensorLine), "%s   %s",
             data.tempStr, data.humidityStr);
    drawText(sensorLine, SENSOR_X, SENSOR_Y + 30, &FONT_SENSOR);
}

void display_init() {
    SPI.end();
    SPI.begin(PIN_EPD_SCK, -1 /* MISO unused */, PIN_EPD_MOSI, PIN_EPD_CS);

    // TODO: check init parameters (baud rate/reset behavior) against the
    // GxEPD2 example sketch for GxEPD2_750_T7, this can vary by panel
    // revision.
    display.init(115200, true, 50, false);
    display.setRotation(0);

    s_ghostingCounter = 0;
}

void display_full_refresh(const DisplayData &data) {
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        drawAllFields(data);
    } while (display.nextPage());

    s_ghostingCounter = 0;
}

void display_partial_update_time(const char *timeStr) {
    display.setPartialWindow(TIME_X, TIME_Y, TIME_W, TIME_H);
    display.firstPage();
    do {
        display.fillRect(TIME_X, TIME_Y, TIME_W, TIME_H, GxEPD_WHITE);
        drawText(timeStr, TIME_X, TIME_Y + 100, &FONT_TIME);
    } while (display.nextPage());

    s_ghostingCounter++;
}

void display_partial_update_info(const DisplayData &data) {
    display.setPartialWindow(INFO_X, INFO_Y, INFO_W, INFO_H + SENSOR_H + (SENSOR_Y - (INFO_Y + INFO_H)));
    display.firstPage();
    do {
        display.fillRect(INFO_X, INFO_Y, INFO_W, INFO_H, GxEPD_WHITE);
        display.fillRect(SENSOR_X, SENSOR_Y, SENSOR_W, SENSOR_H, GxEPD_WHITE);

        char infoLine[48];
        snprintf(infoLine, sizeof(infoLine), "%s, %s  %s",
                 data.weekdayStr, data.dateStr, data.weekStr);
        drawText(infoLine, INFO_X, INFO_Y + 30, &FONT_INFO);

        char sensorLine[24];
        snprintf(sensorLine, sizeof(sensorLine), "%s   %s",
                 data.tempStr, data.humidityStr);
        drawText(sensorLine, SENSOR_X, SENSOR_Y + 30, &FONT_SENSOR);
    } while (display.nextPage());

    s_ghostingCounter++;
}

uint16_t display_get_ghosting_counter() {
    return s_ghostingCounter;
}

bool display_needs_full_refresh() {
    return s_ghostingCounter >= GHOSTING_FULL_REFRESH_THRESHOLD;
}

void display_hibernate() {
    display.hibernate();
}
