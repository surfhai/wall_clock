# Custom fonts

Self-generated Adafruit GFX font headers, see CLAUDE.md section 6. For how
these are generated/regenerated, see **`tools/README.md`** — this file only
covers what's actually here and why.

- **Typeface:** Droid Sans Mono, synthetic bold (see `tools/README.md`).
- **Sizes:** measured pixel-precisely from `docs/Layout.png` (a to-scale
  800x480 mockup) — see `docs/layout.md` for the measurement method and the
  exact glyph-by-glyph numbers. 187px clock digits, 38px week/weekday text,
  53px date/temperature/humidity text.

## Files in this directory

| File               | Font variable                       | Target size | Usage                                | Character set        |
|---------------------|-------------------------------------|-------------|-----------------------------------------|------------------------|
| `MonoClock187.h`    | `droid_sans_mono_bold130pt7b`       | 187 px      | Time (`12:05`)                          | `0123456789:`         |
| `MonoSmall38.h`     | `droid_sans_mono_bold26pt7b`        | 38 px       | Week + weekday (`FONT_SMALL`)           | full ASCII (32–126)   |
| `MonoMedium53.h`    | `droid_sans_mono_bold37pt8b`        | 53 px       | Date + temperature + humidity (`FONT_MEDIUM`) | ASCII + Latin-1 (32–176, includes `°`) |

The variable names (`...130pt7b`, `...26pt7b`, `...37pt8b`) are
`fontconvert`'s internal size parameter for each target pixel height, not
the pixel height itself. Note the `7b`/`8b` suffix: `fontconvert` switches
to `8b` once a font's character range exceeds 127 (needs a full byte) -
`MonoMedium53.h` uses `8b` because its range was widened to 176 for `°`;
`MonoClock187.h` and `MonoSmall38.h` stay `7b` since their ranges (48–58 and
32–126) don't exceed 127.

`MonoMedium53.h` serves three different display lines at once (date,
temperature, humidity) since they all measured out to the same font size -
see `docs/layout.md` for how that was confirmed (comparing per-glyph
cap-height/digit-height, not raw line bounding boxes, which are distorted
by the degree sign and by descenders like the "g" in "Donnerstag").

`formatDisplayData()` in `main.cpp` emits `°` as the raw Latin-1 byte
`0xB0`, not a literal `'°'` in the source (which would be UTF-8-encoded as
two bytes and render as garbage, since GxEPD2/Adafruit_GFX doesn't decode
UTF-8).

**`MonoClock187.h` has the same quirk `MonoClock250.h` (its predecessor at
the old 250px size) had, confirmed:** unusually small *positive* `yOffset`
values in its `GFXglyph` table (e.g. `'1'`: `xOffset=25, yOffset=75`) -
glyphs draw *below* the cursor instead of above it, unlike the other two
fonts here. `src/display.cpp`'s `TIME_X`/`TIME_Y` are deliberately negative
to compensate - see `docs/layout.md` for the calculation.
