#!/bin/sh
# Regenerates the numeric UI fonts from assets/fonts (Montserrat, SIL OFL 1.1).
# Requires Node.js; run from the repository root. Output is committed, so the
# firmware build does not depend on this script.
set -e
CONV="npx --yes lv_font_conv@1.5.3"
TTF=assets/fonts/Montserrat-Bold.ttf
OUT=lib/ui/src/fonts
$CONV --font $TTF --size 150 --bpp 4 --range 0x30-0x39,0x2D --format lvgl --no-compress \
      --lv-include lvgl.h --lv-font-name mk_font_num_150 -o $OUT/mk_font_num_150.c
$CONV --font $TTF --size 80 --bpp 4 --symbols '0123456789.-/ND' --format lvgl --no-compress \
      --lv-include lvgl.h --lv-font-name mk_font_num_80 -o $OUT/mk_font_num_80.c
# Fallback for card values too wide for 80 px.
$CONV --font $TTF --size 52 --bpp 4 --symbols '0123456789.-/ND' --format lvgl --no-compress       --lv-include lvgl.h --lv-font-name mk_font_num_52 -o $OUT/mk_font_num_52.c
# Text fonts: ASCII + Latin-1 (accents and degree sign for pt-BR).
$CONV --font assets/fonts/Montserrat-Medium.ttf --size 20 --bpp 4 --range 0x20-0x7E,0xA0-0xFF --format lvgl --no-compress \
      --lv-include lvgl.h --lv-font-name mk_font_text_20 -o $OUT/mk_font_text_20.c
$CONV --font $TTF --size 28 --bpp 4 --range 0x20-0x7E,0xA0-0xFF --format lvgl --no-compress \
      --lv-include lvgl.h --lv-font-name mk_font_text_28 -o $OUT/mk_font_text_28.c
