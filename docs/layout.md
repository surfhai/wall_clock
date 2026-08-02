# Display layout (800x480)

Based on `docs/Layout.png`, a to-scale 800x480 mockup (matches the real
panel resolution exactly, so pixel positions below are 1:1, not scaled).
The values below mirror the coordinates in `src/display.cpp` exactly — keep
**both** places in sync when changing anything.

```
0,0 ─────────────────────────────────────────────────────── 800,0
│                                                              │
│  12:05                                                       │  <- FONT_TIME, 187px
│                                                              │
│  KW32                  24,3°C                                │  <- FONT_SMALL / FONT_MEDIUM, 38/53px
│  Donnerstag            55%rLF                                │  <- FONT_SMALL / FONT_MEDIUM, 38/53px
│  2026-07-28                                                  │  <- FONT_MEDIUM, 53px
│                                                              │
0,480 ─────────────────────────────────────────────────── 800,480
```

(The sample text is German, the default display language — see
`DISPLAY_LANGUAGE` in `include/config.h`.)

## Font sizes — measured pixel-precisely from docs/Layout.png

Measured via a small Python/Pillow script: threshold the PNG to black/white,
find each line's ink bounding box, then split into individual glyph columns
to measure **cap-height/digit-height specifically** — not the raw line
bounding box, which is distorted by descenders (e.g. the "g" in
"Donnerstag" extends 11px below the baseline) and by the degree sign (which
sits higher than a normal capital letter, inflating "24,3°C"'s raw bbox to
69px even though its actual digit height matches the other medium-size
text).

| Element | Glyphs measured | Height | Font role |
|---|---|---|---|
| `12:05` | (not needed - already at the 255px ceiling regardless, see CLAUDE.md section 6) | 187px | `FONT_TIME` |
| `KW32` | K, 3, 2 | 38px (all three exactly) | `FONT_SMALL` |
| `Donnerstag` | capital "D" only (skips the lowercase x-height letters and the descending "g") | 38px | `FONT_SMALL` |
| `2026-07-28` | all 8 digits (skips the two '-') | 51-53px | `FONT_MEDIUM` |
| `24,3°C` | 2, 4, 3, C (skips the comma and the degree sign) | 51-54px | `FONT_MEDIUM` |
| `55%rLF` | 5, 5, %, L, F (skips lowercase "r", x-height only) | 52px | `FONT_MEDIUM` |

**KW32's digits/letters and "Donnerstag"'s capital "D" both measure exactly
38px** — confirms they're the same font size despite the visually taller
line bounding box for "Donnerstag" (49px, purely due to the "g" descender).
Similarly, the date and both sensor lines all measure 51-54px (differences
within font-rendering/rounding noise) — confirms one shared "medium" font
size of 53px for date + temperature + humidity, per the explicit design
decision (see conversation this was derived from).

`tools/generate_fonts.sh` uses these three target heights: `CLOCK_PX=187`,
`SMALL_PX=38`, `MEDIUM_PX=53`.

## Coordinates

All (x, y) are the Adafruit GFX cursor position (left edge, baseline) for
`display.setCursor()` + `print()`. Elements meant to align share the same
X: week/weekday/date all start at `LEFT_X=17`; temperature/humidity both
start at `RIGHT_X=526`.

| Element | Font | x | y |
|---|---|---|---|
| Time (`12:05`) | `FONT_TIME` | -1 | -41 (negative on purpose, see caveat below) |
| Week (`KW32`) | `FONT_SMALL` | 17 | 320 (baseline) |
| Weekday (`Donnerstag`) | `FONT_SMALL` | 17 | 387 (baseline) |
| Date (`2026-07-28`) | `FONT_MEDIUM` | 17 | 468 (baseline) |
| Temperature (`24,3°C`) | `FONT_MEDIUM` | 526 | 359 (baseline) |
| Humidity (`55%rLF`) | `FONT_MEDIUM` | 526 | 464 (baseline) |

Partial-refresh erase boxes group these into three regions (see
`src/display.cpp`): `TIME_BOX_*` (time only), `LEFT_BOX_*` (week + weekday +
date), `RIGHT_BOX_*` (temperature + humidity) — sized with a few pixels of
margin beyond the measured text extents.

## Confirmed: `MonoClock187.h`'s cursor behavior

Like its 250px predecessor, `MonoClock187.h` has unusually small
**positive** `yOffset` values in its `GFXglyph` table — confirmed after
generating it: `'1'` (the first character of "12:05") has `xOffset=25,
yOffset=75`. Every other font here behaves normally (negative `yOffset`,
cursor = baseline).

Since `drawChar()` draws the glyph bitmap's top-left corner at
`(cursor_x + xOffset, cursor_y + yOffset)`, matching the measured mockup
position (glyph top-left at `x=24, y=34`) requires a **deliberately
negative cursor**: `cursor = target - offset`, i.e.
`TIME_X = 24 - 25 = -1` and `TIME_Y = 34 - 75 = -41`. The cursor value
itself being negative is fine - only the actual drawn pixels
(`cursor + offset`) need to land on-screen, and they do (verified against
`'1'`'s specific offsets; other digits' offsets vary by 1-3px, an
acceptable/expected margin for a proportional-metrics font).

## TODOs

- Verify all elements render correctly on the real display and adjust
  coordinates if needed - these are transcribed from a mockup image plus
  font-metrics math, not yet confirmed on actual hardware.
- Once final: update this document and the constants in `src/display.cpp`
  together.
