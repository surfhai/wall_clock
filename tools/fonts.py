#!/usr/bin/env python3
"""
fonts.py — all font-generation helper logic for this project in one file,
dispatched by subcommand, instead of one script per task. See
tools/README.md for the overall workflow; generate_fonts.sh is the
orchestrator that calls these.

Subcommands:
    px-to-size <font.ttf> <target_px> [ref_char]
        Prints the fontconvert "size" parameter needed to reach a target
        pixel height for a given reference glyph (fontconvert assumes a
        fixed 141 DPI, so its size argument is not itself a pixel value).

    clamp-yadvance <header.h>
        Clamps an out-of-range GFXfont yAdvance field to 255 (uint8_t max)
        in a fontconvert-generated header, in place. yAdvance is a
        font-wide line-height metric that scales with pixel size (unlike
        glyph width/height), so it can overflow well before glyphs do -
        this project never uses it (no multi-line text), so clamping is
        safe.

    make-bold <regular.ttf> <bold.ttf> [stroke]
        Generates a synthetic bold TTF from a regular-weight TTF via
        FontForge's changeWeight(). Must be run under `fontforge -script`,
        not plain python3 (needs the `fontforge` module). `stroke`
        defaults to 40 (worked well for Droid Sans Mono - adjust if a
        different typeface looks too thin/thick).
"""
import re
import sys

_YADVANCE_PATTERN = re.compile(r"(0x[0-9A-Fa-f]+,\s*0x[0-9A-Fa-f]+,\s*)(\d+)(\s*\};)")


def px_to_fontsize(ttf_path, target_px, ref_char="0"):
    from fontTools.ttLib import TTFont

    dpi = 141  # matches #define DPI 141 in fontconvert.c

    f = TTFont(ttf_path)
    upm = f["head"].unitsPerEm
    cmap = f.getBestCmap()
    glyf = f["glyf"]

    cp = ord(ref_char)
    if cp not in cmap:
        raise ValueError(f"Reference char '{ref_char}' not found in font")

    g = glyf[cmap[cp]]
    height_units = g.yMax - g.yMin

    # size_param * dpi / (72 * upm) = px per font-unit
    # target_px = height_units * px_per_unit  -> solve for size_param
    size_param = target_px * 72 * upm / (dpi * height_units)
    return round(size_param)


def clamp_yadvance(header_path):
    with open(header_path) as f:
        content = f.read()

    match = _YADVANCE_PATTERN.search(content)
    if not match:
        print(f"WARNING: {header_path}: could not find the GFXfont trailer, leaving as-is.",
              file=sys.stderr)
        return

    yadvance = int(match.group(2))
    if yadvance <= 255:
        return

    print(f"NOTE: {header_path}: yAdvance {yadvance} exceeds the uint8_t limit - "
          f"clamping to 255 (unused by this project, see include/fonts/README.md).",
          file=sys.stderr)

    replacement = (f"{match.group(1)}255{match.group(3)}"
                   f"  // clamped from {yadvance}, see include/fonts/README.md")
    content = content[:match.start()] + replacement + content[match.end():]

    with open(header_path, "w") as f:
        f.write(content)


def make_bold(regular_ttf, bold_ttf, stroke=40):
    import fontforge

    f = fontforge.open(regular_ttf)
    for glyph in f.glyphs():
        glyph.changeWeight(stroke, "LCG", 0, 0, "squish", True)

    f.familyname += " Bold"
    f.fontname += "-Bold"
    f.fullname += " Bold"
    f.os2_weight = 700

    f.generate(bold_ttf)


def _usage(msg=None):
    if msg:
        print(msg, file=sys.stderr)
    print(__doc__, file=sys.stderr)
    sys.exit(1)


def main(argv):
    if len(argv) < 2:
        _usage()

    cmd, args = argv[1], argv[2:]

    if cmd == "px-to-size":
        if len(args) < 2:
            _usage("Usage: fonts.py px-to-size <font.ttf> <target_px> [ref_char]")
        ref_char = args[2] if len(args) > 2 else "0"
        print(px_to_fontsize(args[0], float(args[1]), ref_char))

    elif cmd == "clamp-yadvance":
        if len(args) < 1:
            _usage("Usage: fonts.py clamp-yadvance <header.h>")
        clamp_yadvance(args[0])

    elif cmd == "make-bold":
        if len(args) < 2:
            _usage("Usage: fontforge -script fonts.py make-bold <regular.ttf> <bold.ttf> [stroke]")
        stroke = int(args[2]) if len(args) > 2 else 40
        make_bold(args[0], args[1], stroke)

    else:
        _usage(f"Unknown subcommand: {cmd}")


if __name__ == "__main__":
    main(sys.argv)
