#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "usage: $0 <NotoSansKR-Regular.otf> <lv_font_conv>" >&2
    exit 2
fi

font_path=$1
converter=$2
expected_font_sha=69975a0ac8472717870aefeab0a4d52739308d90856b9955313b2ad5e0148d68
repo_root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
output="$repo_root/source/graphics/fonts/lv_font_noto_sans_kr_16_ko.c"
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT

actual_font_sha=$(sha256sum "$font_path" | cut -d ' ' -f 1)
if [[ "$actual_font_sha" != "$expected_font_sha" ]]; then
    echo "unexpected NotoSansKR-Regular.otf SHA-256: $actual_font_sha" >&2
    exit 1
fi

converter_version=$($converter --version)
if [[ "$converter_version" != "1.5.3" ]]; then
    echo "expected lv_font_conv 1.5.3, got $converter_version" >&2
    exit 1
fi

cp "$font_path" "$build_dir/NotoSansKR-Regular.otf"
(
    cd "$build_dir"
    "$converter" \
        --font NotoSansKR-Regular.otf \
        --range 0xAC00-0xD7A3,0x3131-0x318E \
        --size 16 \
        --bpp 2 \
        --format lvgl \
        --lv-include lvgl.h \
        --lv-font-name lv_font_noto_sans_kr_16_ko \
        --no-kerning \
        --no-compress \
        --no-prefilter \
        --output lv_font_noto_sans_kr_16_ko.c
)

cp "$build_dir/lv_font_noto_sans_kr_16_ko.c" "$output"
perl -0pi -e 's/\s+\z/\n/' "$output"
echo "generated $output"
