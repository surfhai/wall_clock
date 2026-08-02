#include "display.h"
#include "config.h"

#include <SPI.h>
// GxEPD2_BW.h already conditionally includes all panel drivers (including
// epd/GxEPD2_750_T7.h) - no separate include needed.
// TODO: verify the panel print on the FPC cable side and switch to
// GxEPD2_750c_Z8 if needed (see CLAUDE.md section 4/10).
#include <GxEPD2_BW.h>

// Custom fonts generated via tools/generate_fonts.sh from Droid Sans Mono
// Bold (see include/fonts/README.md), per CLAUDE.md section 6. Sizes were
// measured pixel-precisely from docs/Layout.png, see docs/layout.md.
#include "fonts/MonoClock187.h"
#include "fonts/MonoSmall38.h"
#include "fonts/MonoMedium53.h"

#define FONT_TIME    droid_sans_mono_bold130pt7b  // MonoClock187.h, digits + ':'
#define FONT_SMALL   droid_sans_mono_bold26pt7b   // MonoSmall38.h, full ASCII - week + weekday
#define FONT_MEDIUM  droid_sans_mono_bold37pt8b   // MonoMedium53.h, ASCII + Latin-1 - date + temp + humidity

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
// Layout — each piece of information gets its own (x, y) cursor position,
// measured pixel-precisely from docs/Layout.png (an 800x480 to-scale
// mockup); see docs/layout.md for the measurement method. (x, y) is the
// Adafruit GFX cursor position (baseline, left edge) for that string.
//
// Elements meant to align share the same X (week/weekday/date: left column,
// all at WEEK_X; temperature/humidity: right column, both at TEMP_X) or the
// same general row grouping, per the mockup.
// ---------------------------------------------------------------------------
// MonoClock187.h confirmed to have the same quirk as its 250px predecessor:
// positive GFXglyph yOffset (e.g. '1': xOffset=25, yOffset=75 - glyphs draw
// *below* the cursor, not above it). TIME_X/TIME_Y are back-calculated from
// '1' (the first character of "12:05") so the glyph top lands at the
// measured mockup position (x=24, y=34, see docs/layout.md): cursor = target
// - offset, i.e. deliberately negative. This is correct, not a bug - the
// actual drawn pixels (cursor + offset) still land on-screen.
static const int16_t TIME_X = -1, TIME_Y = -41;

static const int16_t LEFT_X = 17;                // shared left edge: week/weekday/date
static const int16_t WEEK_Y = 320;               // "KW32" / "CW32"
static const int16_t WEEKDAY_Y = 387;            // "Donnerstag" / "Thursday"
static const int16_t DATE_Y = 468;               // "2026-07-30"

static const int16_t RIGHT_X = 526;              // shared left edge: temperature/humidity
static const int16_t TEMP_Y = 359;               // "24,3°C"
static const int16_t HUMIDITY_Y = 464;           // "55%rLF"

// Erase/partial-refresh regions. TIME gets its own box; WEEK/WEEKDAY/DATE
// and TEMP/HUMIDITY are erased+redrawn together as two side-by-side blocks
// (cheaper than 5 separate partial windows, and they're always updated
// together anyway - see main.cpp's tier logic).
static const int16_t TIME_BOX_X = 10, TIME_BOX_Y = 10, TIME_BOX_W = 780, TIME_BOX_H = 230;
static const int16_t LEFT_BOX_X = 10, LEFT_BOX_Y = 278, LEFT_BOX_W = 450, LEFT_BOX_H = 200;
static const int16_t RIGHT_BOX_X = 520, RIGHT_BOX_Y = 298, RIGHT_BOX_W = 270, RIGHT_BOX_H = 180;

static void drawText(const char *text, int16_t x, int16_t y, const GFXfont *font) {
    display.setFont(font);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(x, y);
    display.print(text);
}

static void drawTime(const char *timeStr) {
    // See the TIME_X/TIME_Y comment above re: the negative cursor position
    // - this is intentional, not a mistake.
    drawText(timeStr, TIME_X, TIME_Y, &FONT_TIME);
}

static void drawInfoBlock(const DisplayData &data) {
    drawText(data.weekStr, LEFT_X, WEEK_Y, &FONT_SMALL);
    drawText(data.weekdayStr, LEFT_X, WEEKDAY_Y, &FONT_SMALL);
    drawText(data.dateStr, LEFT_X, DATE_Y, &FONT_MEDIUM);
}

static void drawSensorBlock(const DisplayData &data) {
    drawText(data.tempStr, RIGHT_X, TEMP_Y, &FONT_MEDIUM);
    drawText(data.humidityStr, RIGHT_X, HUMIDITY_Y, &FONT_MEDIUM);
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
        drawTime(data.timeStr);
        drawInfoBlock(data);
        drawSensorBlock(data);
    } while (display.nextPage());

    s_ghostingCounter = 0;
}

void display_partial_update_time(const char *timeStr) {
    display.setPartialWindow(TIME_BOX_X, TIME_BOX_Y, TIME_BOX_W, TIME_BOX_H);
    display.firstPage();
    do {
        display.fillRect(TIME_BOX_X, TIME_BOX_Y, TIME_BOX_W, TIME_BOX_H, GxEPD_WHITE);
        drawTime(timeStr);
    } while (display.nextPage());

    s_ghostingCounter++;
}

void display_partial_update_info(const DisplayData &data) {
    // One combined window spanning both blocks (with the gap between them)
    // - one partial refresh instead of two, see CLAUDE.md section 7.
    int16_t x = LEFT_BOX_X;
    int16_t y = (LEFT_BOX_Y < RIGHT_BOX_Y) ? LEFT_BOX_Y : RIGHT_BOX_Y;
    int16_t right = RIGHT_BOX_X + RIGHT_BOX_W;
    int16_t bottom = (LEFT_BOX_Y + LEFT_BOX_H > RIGHT_BOX_Y + RIGHT_BOX_H)
                          ? LEFT_BOX_Y + LEFT_BOX_H
                          : RIGHT_BOX_Y + RIGHT_BOX_H;

    display.setPartialWindow(x, y, right - x, bottom - y);
    display.firstPage();
    do {
        display.fillRect(LEFT_BOX_X, LEFT_BOX_Y, LEFT_BOX_W, LEFT_BOX_H, GxEPD_WHITE);
        display.fillRect(RIGHT_BOX_X, RIGHT_BOX_Y, RIGHT_BOX_W, RIGHT_BOX_H, GxEPD_WHITE);
        drawInfoBlock(data);
        drawSensorBlock(data);
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
