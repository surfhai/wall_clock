# Font generation

How to (re)generate the Adafruit GFX font headers used by this project
(see `include/fonts/README.md` for which files exist and what they're used
for). All font-generation logic lives in **one file**, `fonts.py`, as
subcommands (`px-to-size`, `clamp-yadvance`, `make-bold`) rather than one
script per task — see its module docstring for the exact usage of each.
`generate_fonts.sh` is the orchestrator that calls into it.

## Prerequisites

- `fontconvert` built once, from
  [Adafruit-GFX-Library](https://github.com/adafruit/Adafruit-GFX-Library)/`fontconvert`
  (`make`, requires WSL/Linux + FreeType).
- `fonttools` installed: `pip install fonttools --break-system-packages`.
- Only needed if you don't already have a bold TTF (see below): FontForge —
  `sudo apt install fontforge python3-fontforge` (WSL).

## Regenerating with the current typeface

```bash
./generate_fonts.sh
```

Edit the **target pixel heights** at the top of the script (`CLOCK_PX`,
`SMALL_PX`, `MEDIUM_PX`) — not the `fontconvert` size argument. These were
measured pixel-precisely from a layout mockup, see `docs/layout.md`. The
script uses `fonts.py px-to-size` to compute the correct `fontconvert` size
parameter for each target height, since `fontconvert`'s size argument is
not itself a pixel value (see that subcommand's docstring — it assumes a
fixed 141 DPI). It also enforces the 255 px `GFXglyph` limit on the target
heights and prints the computed sizes before running.

After each `fontconvert` call, `fonts.py clamp-yadvance` runs automatically
and clamps the generated header's `GFXfont.yAdvance` field to 255 if it
overflows. Unlike glyph width/height, `yAdvance` is a font-wide line-height
metric that scales with pixel size (ascent+descent+line-gap from the TTF),
so it can exceed the `uint8_t` limit well before glyphs do — this happened
at the clock size (187px and the previous 250px both overflow it). This
project never relies on `yAdvance` (no multi-line `\n` text, every string
is drawn as a single line at a manually positioned cursor), so clamping it
is safe and needs no manual follow-up after regenerating.

**Note:** actual rendered glyph height can be off by a pixel or two due to
rounding — check the `'0'` glyph's `height` field in the generated `.h` if
exact px matters.

## Starting from a different typeface

1. Get the TTF (regular and/or bold weight).
2. If there's no bold variant, generate a synthetic one via FontForge:
   ```bash
   fontforge -script fonts.py make-bold MyFont-Regular.ttf MyFont-Bold.ttf
   ```
   (Optional 4th arg overrides the `changeWeight()` stroke value, default
   40 - start around 35–40 and adjust until it looks right; 40 was used
   for Droid Sans Mono.)
3. Point `FONT_TTF` in `generate_fonts.sh` at the (bold) TTF and set the
   target pixel heights for your layout.
4. Run `./generate_fonts.sh` as above.
5. Update the includes and `FONT_TIME`/`FONT_SMALL`/`FONT_MEDIUM` in
   `src/display.cpp` to match the new generated variable names.
