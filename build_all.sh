#!/usr/bin/env bash
# Build itachi-hal (HAL + FOD/SOFOD + license check) with Android NDK r25+. (c) @yooitachiijod
set -euo pipefail
: "${NDK:?set NDK=/path/to/android-ndk}"
[ -f hal/itachi_sealed.h ] || { echo "hal/itachi_sealed.h missing. Run first:
  python3 tools/itachi_license.py keygen
  python3 tools/itachi_license.py sign --name @yooitachiijod --device duchamp
  python3 tools/itachi_license.py seal"; exit 1; }
API="${API:-34}"
HOST=$(ls "$NDK/toolchains/llvm/prebuilt" | head -1)
BIN="$NDK/toolchains/llvm/prebuilt/$HOST/bin"
OUT=out; mkdir -p "$OUT"
"$BIN/aarch64-linux-android${API}-clang" -std=gnu11 -O2 -Wall -Wextra -fPIE -pie -fvisibility=hidden \
  -Ihal -Ithird_party -o "$OUT/itachi-hal" \
  hal/itachi_hal.c hal/itachi_fod.c hal/itachi_license.c third_party/tweetnacl.c \
  -lbinder_ndk -llog -ldl -lpthread
"$BIN/llvm-strip" "$OUT/itachi-hal"
[ -f license.lic ] && cp license.lic "$OUT/license.lic"
echo "built $OUT/itachi-hal  (+ out/license.lic)"
