# Custom fonts (placeholder)

This directory is the target location for the self-generated Adafruit GFX
font headers, see CLAUDE.md section 6. Nothing has been generated yet —
`src/display.cpp` temporarily uses built-in Adafruit GFX standard fonts
(`Fonts/FreeSansBold24pt7b.h` etc.) so the project already compiles and can
be tested.

## Open decisions (see CLAUDE.md section 10)

1. **Typeface:** Space Mono / JetBrains Mono / Courier Prime / IBM Plex Mono
   (monospaced, OFL-licensed)
2. **Solution for clock digits > 255 px** (GFXglyph limit):
   - stay just under 255 px (e.g. 250 px), or
   - draw a seven-segment style yourself, or
   - pre-rendered bitmaps per digit

## Workflow once both are decided

```bash
git clone https://github.com/adafruit/Adafruit-GFX-Library
cd Adafruit-GFX-Library/fontconvert
make    # requires freetype

./fontconvert <font>.ttf <pixel_size> <ascii_start> <ascii_end> > <Name><Size>.h
```

See `tools/generate_fonts.sh` in this project for a batch script that
generates all combinations needed below in one go (adjust the paths in it
once the font file and target sizes are final).

## Required font sizes (see CLAUDE.md section 6)

| File (placeholder name)   | Usage                   | Character set                 |
|----------------------------|-------------------------|-------------------------------|
| `MonoClockXXX.h`           | Time (12:05)            | `0123456789:`                |
| `MonoDateXX.h`              | Date                     | `0123456789-`                |
| `MonoSensorXX.h`            | Temperature/humidity     | `0123456789,%°CrLF`          |
| `MonoTextXX.h`               | Week + weekday           | `A-Z`, umlauts (Ä/Ö/Ü), `0-9` |

After generating: switch the includes and `FONT_TIME`/`FONT_INFO`/
`FONT_SENSOR` in `src/display.cpp` to the new headers.
