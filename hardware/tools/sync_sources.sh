#!/usr/bin/env bash
# Copies the cipher sources from ../../src into the Arduino sketch folder.
# Why: the Arduino IDE only builds files that live inside the sketch folder, and it ignores *.inc files,
# so present_key.inc is copied as present_key.h (and the #include lines are patched in the COPIES only).
# The copies are git-ignored: src/ stays the single source of truth.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
DST="$ROOT/hardware/arduino/cipher_bench"

[ -f "$ROOT/src/speck.c" ] || { echo "cannot find $ROOT/src - run this from a checkout of the project" >&2; exit 1; }
[ -f "$ROOT/src/present_tables.h" ] || python3 "$ROOT/tools/gen_tables.py"

for f in speck.c speck.h speck_asm.S present.h present_opt.c present_naive.c \
         chacha.c chacha.h vectors.h present_tables.h; do
  cp "$ROOT/src/$f" "$DST/$f"
done
cp "$ROOT/src/present_key.inc" "$DST/present_key.h"
sed -i 's/present_key\.inc/present_key.h/' "$DST/present_opt.c" "$DST/present_naive.c"

echo "synced $(ls "$DST" | wc -l) files into hardware/arduino/cipher_bench"
