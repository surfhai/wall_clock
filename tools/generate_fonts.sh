#!/usr/bin/env bash
# generate_fonts.sh — generates all required font headers in one go.
#
# Prerequisite: Adafruit-GFX-Library/fontconvert has already been built,
# and fonttools is installed (pip install fonttools --break-system-packages).
#
# Define TARGET PIXEL heights below — the script computes the correct
# fontconvert "size" parameter for you (fontconvert's size argument is
# NOT pixels, see fonts.py's px-to-size subcommand for the conversion).
#
# Target heights below were measured pixel-precisely from docs/Layout.png
# (a to-scale 800x480 mockup) via cap-height/digit-height, ignoring
# ascenders/descenders (e.g. the 'g' in "Donnerstag") and the degree sign
# (which sits higher than normal letters) - see docs/layout.md for the
# measurement method and exact glyph-by-glyph numbers.
#
# All font-generation logic (px-to-size, yAdvance clamping, synthetic bold)
# lives in ./fonts.py as subcommands - this script just orchestrates calls
# into it instead of shelling out to several separate one-off scripts.
set -euo pipefail

FONTCONVERT="../Adafruit-GFX-Library/fontconvert/fontconvert"
FONT_TTF="droid-sans-mono-bold.ttf"
OUT_DIR="../include/fonts"
FONTS_PY="./fonts.py"

mkdir -p "$OUT_DIR"

# --- Target pixel heights (edit these, not the fontconvert size!) ---
CLOCK_PX=187   # time "12:05" digit height
SMALL_PX=38    # week ("KW32") + weekday ("Donnerstag") lines
MEDIUM_PX=53   # date + temperature + humidity lines

# Reference glyph used to measure height. '0' fits all blocks here since
# this is a monospace digit-heavy layout.
REF_CHAR="0"

# Safety check: GFXglyph height field is uint8_t (max 255px)
for px in "$CLOCK_PX" "$SMALL_PX" "$MEDIUM_PX"; do
    if (( px > 255 )); then
        echo "ERROR: target height ${px}px exceeds the 255px GFXglyph uint8_t limit." >&2
        exit 1
    fi
done

calc_size() {
    python3 "$FONTS_PY" px-to-size "$FONT_TTF" "$1" "$REF_CHAR"
}

CLOCK_SIZE=$(calc_size "$CLOCK_PX")
SMALL_SIZE=$(calc_size "$SMALL_PX")
MEDIUM_SIZE=$(calc_size "$MEDIUM_PX")

echo "Target ${CLOCK_PX}px  -> fontconvert size ${CLOCK_SIZE}"
echo "Target ${SMALL_PX}px   -> fontconvert size ${SMALL_SIZE}"
echo "Target ${MEDIUM_PX}px   -> fontconvert size ${MEDIUM_SIZE}"

"$FONTCONVERT" "$FONT_TTF" "$CLOCK_SIZE"  48 58  > "$OUT_DIR/MonoClock${CLOCK_PX}.h"   # 0-9 + ':'
# 32-126: full ASCII - the weekday line ("Donnerstag"/"Sunday" etc.) needs
# the full alphabet, the week line ("KW32") just digits + a couple letters.
"$FONTCONVERT" "$FONT_TTF" "$SMALL_SIZE"  32 126 > "$OUT_DIR/MonoSmall${SMALL_PX}.h"   # week + weekday
# 32-176 (not 126): this font covers the date line AND both sensor value
# lines (see src/display.cpp, FONT_MEDIUM points here) - the temperature
# line needs the degree sign '°' (U+00B0 = decimal 176), which sits outside
# 7-bit ASCII. This pulls in the Latin-1 punctuation/symbol block between
# 127-175 too (unused), since fontconvert only supports one contiguous
# codepoint range per call.
"$FONTCONVERT" "$FONT_TTF" "$MEDIUM_SIZE" 32 176 > "$OUT_DIR/MonoMedium${MEDIUM_PX}.h" # date + temp + humidity
# TODO: if umlauts (Ä/Ö/Ü/ä/ö/ü, see CLAUDE.md section 6) are ever needed
# for weekday names, widen MonoSmall's range up to 252 (ü). Not done by
# default since the current weekday names (German and English) don't use
# any.

# yAdvance is a font-wide line-height metric that scales with target pixel
# size (unlike glyph width/height, which stay within the 255px check above)
# - at large sizes it can overflow the uint8_t field and fail to compile.
# This project never uses it (no multi-line text), so any overflow is
# clamped to 255 automatically here instead of failing the build.
for f in "$OUT_DIR/MonoClock${CLOCK_PX}.h" "$OUT_DIR/MonoSmall${SMALL_PX}.h" \
         "$OUT_DIR/MonoMedium${MEDIUM_PX}.h"; do
    python3 "$FONTS_PY" clamp-yadvance "$f"
done

echo "Done. Update the includes/font constants in src/display.cpp."
echo "Note: actual rendered height can be off by a pixel or two due to rounding — check the '0' glyph's height field in the generated .h if exact px matters."
