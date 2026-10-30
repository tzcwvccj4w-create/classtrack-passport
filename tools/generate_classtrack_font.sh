#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
converter="${LV_FONT_CONV:-lv_font_conv}"
font_source="$repo_root/assets/fonts/SourceHanSansSC-Normal.otf"
if [[ ! -f "$font_source" ]]; then
  echo "Place SourceHanSansSC-Normal.otf in assets/fonts before regenerating the subset." >&2
  exit 1
fi
symbols="$(tr -d '\r\n' < "$repo_root/assets/fonts/classtrack-glyphs.txt")"

"$converter" \
  --size 16 --bpp 4 --format lvgl \
  --font "$font_source" \
  --range 0x20-0x7E --symbols "$symbols" \
  --no-compress --no-prefilter --force-fast-kern-format \
  --lv-include lvgl.h --lv-font-name classtrack_font_16 \
  --output "$repo_root/assets/fonts/classtrack_font_16.c"
