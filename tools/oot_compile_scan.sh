#!/bin/sh
# How much of the vendored decomp builds for ARM9 without being touched.
#
# Run from the project root inside MSYS2. The point is not to compile the game
# yet: it is to measure what the toolchain and the SDK headers already agree on,
# and to group what they do not, so the compatibility layer gets built against a
# list of real gaps instead of a guess.
#
#   sh tools/oot_compile_scan.sh [report-file]      # default /tmp/oot_scan.txt
#
# Only failures are written out, one line each, with the first error the
# compiler reported. The last line is the tally.

set -u

DEVKITARM=${DEVKITARM:-/opt/devkitpro/devkitARM}
CC="$DEVKITARM/bin/arm-none-eabi-gcc"
SRC=${SRC_DIR:-game/oot/src}
OUT=${1:-/tmp/oot_scan.txt}

# _LANGUAGE_C is the switch the Nintendo SDK headers key every type on; without
# it ultra64/ultratypes.h declares nothing at all. NON_MATCHING turns off the
# decomp's own "this must line up with the retail MIPS" assertions, which are
# about a target we are not building for. OOT_VERSION/REVISION, DEBUG_FEATURES,
# PLATFORM_N64 and F3DEX_GBI_2 are the defines the decomp's own Makefile always
# passes (lines 243, 414 and 426 there): without the first two `line_numbers.h`
# refuses to expand, PLATFORM_N64 selects the arena layout the NTSC `__osMalloc`
# expects, and F3DEX_GBI_2 is what makes `gSP2Triangles` and `G_TRI2` exist —
# the same ucode the display list decoder in source/gfx implements.
#
# -I include/libc is the decomp's own shim set (assert.h, math.h, string.h …).
# It is what supplies ASSERT and SHT_MAX; the price is that `#include <string.h>`
# now finds the decomp's declaration-only header instead of newlib's, so the
# implementations have to come from elsewhere. See source/oot/oot_compat.h.
CFLAGS="-fsyntax-only -O0 -std=gnu17 -march=armv5te -mtune=arm946e-s -w"
CFLAGS="$CFLAGS -D_LANGUAGE_C -D_MIPS_SZPTR=32 -DNON_MATCHING"
CFLAGS="$CFLAGS -DOOT_VERSION=NTSC_1_0 -DOOT_REVISION=0 -DDEBUG_FEATURES=0"
CFLAGS="$CFLAGS -DPLATFORM_N64=1 -DF3DEX_GBI_2"
CFLAGS="$CFLAGS -I game/oot/include -I game/oot/include/libc -I game/oot"
CFLAGS="$CFLAGS ${EXTRA_CFLAGS:-}"

if [ ! -x "$CC" ]; then
    echo "no compiler at $CC" >&2
    exit 1
fi

: > "$OUT"
pass=0
fail=0
total=0

for f in $(find "$SRC" -name '*.c' | sort); do
    total=$((total + 1))
    if err=$("$CC" $CFLAGS "$f" 2>&1); then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
        printf '%s :: %s\n' "$f" "$(printf '%s' "$err" | grep -m1 'error:' | tr -s ' ')" >> "$OUT"
    fi
done

printf 'total=%d pass=%d fail=%d\n' "$total" "$pass" "$fail" >> "$OUT"
printf 'total=%d pass=%d fail=%d  (%s)\n' "$total" "$pass" "$fail" "$OUT"
