# Display layout (800x480) — placeholder

No final layout sketch yet (see CLAUDE.md section 10). The values below
mirror the current placeholder coordinates in `src/display.cpp` exactly —
keep **both** places in sync when changing anything.

```
0,0 ─────────────────────────────────────────────────── 800,0
│                                                         │
│  [ TIME  ]  x=40  y=30  w=500 h=160   "12:05"           │
│                                                         │
│  [ INFO  ]  x=40  y=200 w=720 h=140   "Donnerstag, 2026-07-30  KW32" │
│                                                         │
│  [ SENSOR]  x=40  y=360 w=720 h=100   "24,3C   55%rLF"  │
│                                                         │
0,480 ───────────────────────────────────────────────── 800,480
```

(The sample text above is German, the default display language — see
`DISPLAY_LANGUAGE` in `include/config.h`.)

TODOs:
- Determine the actual positioning/sizes after a visual test on the real
  display (margins, centering, font sizes per field).
- Once final: update this document and the constants in `src/display.cpp`
  together.
