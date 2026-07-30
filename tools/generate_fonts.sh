#!/usr/bin/env bash
# generate_fonts.sh — generates all required font headers in one go.
#
# Prerequisite: Adafruit-GFX-Library/fontconvert has already been built
# (see include/fonts/README.md). Only run once the typeface and final pixel
# sizes have been decided (see CLAUDE.md section 6/10, TODOs).
set -euo pipefail

# TODO: adjust the path to the built fontconvert binary.
FONTCONVERT="../Adafruit-GFX-Library/fontconvert/fontconvert"

# TODO: TTF file of the final chosen typeface (see include/fonts/README.md).
FONT_TTF="SpaceMono-Bold.ttf"

OUT_DIR="../include/fonts"

# TODO: finalize the pixel size for the clock (see CLAUDE.md section 6 —
# mind the 255px uint8_t limit, e.g. 250 instead of 267).
"$FONTCONVERT" "$FONT_TTF" 250 48 58  > "$OUT_DIR/MonoClock250.h"   # 0-9 + ':'
"$FONTCONVERT" "$FONT_TTF" 60  32 126 > "$OUT_DIR/MonoDate60.h"     # date
"$FONTCONVERT" "$FONT_TTF" 50  32 126 > "$OUT_DIR/MonoSensor50.h"   # temp/humidity
"$FONTCONVERT" "$FONT_TTF" 50  32 126 > "$OUT_DIR/MonoText50.h"     # week/weekday

echo "Done. Update the includes/font constants in src/display.cpp."
