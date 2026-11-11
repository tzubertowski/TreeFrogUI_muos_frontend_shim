#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "$0")/.." && pwd)
TOOLCHAIN=${SF3000_TOOLCHAIN:-/home/tomaszz/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot}
CC=${MIPSEL_CC:-$TOOLCHAIN/opt/ext-toolchain/bin/mips-mti-linux-gnu-gcc}
SYSROOT=${MIPSEL_SYSROOT:-$TOOLCHAIN/mipsel-buildroot-linux-gnu/sysroot}
QEMU=${QEMU_MIPSEL:-qemu-mipsel-static}
SDL2_INCLUDE=${SDL2_INCLUDE:-/home/tomaszz/sf3000-work/FroggyDuke/thirdparty/include}
THEME_ARCHIVE=${MUOS_THEME_ARCHIVE:-$ROOT/assets/MinUIfied-Horizontal.muxthm}
test -s "$THEME_ARCHIVE" || { echo "Set MUOS_THEME_ARCHIVE to MinUIfied Horizontal .muxthm" >&2; exit 1; }
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
OPT="$TMP/opt/muos/"
RUN="$TMP/run/muos/"
LAUNCH="$TMP/frogui_launch.txt"
mkdir -p "$OPT/share/theme/MustardOS" "$OPT/share/font/internal" \
    "$RUN/storage/theme/MinUIfied.-.Horizontal" "$RUN/storage/info"
unzip -oq "$THEME_ARCHIVE" -d "$RUN/storage/theme/MinUIfied.-.Horizontal"
printf 'Edge\n' > "$OPT/share/theme/MustardOS/version.txt"

MUOS_OPT_PATH=$OPT MUOS_RUN_PATH=$RUN MUOS_HOST_LAUNCH_FILE=$LAUNCH MUOS_HOST_TEST=1 \
    "$ROOT/bridge/build_libretro.sh" "$TMP/muos-core.so"
"$CC" -std=gnu11 -mips32r2 -mhard-float -EL --sysroot="$SYSROOT" \
    -DOPT_PATH="\"$OPT\"" -DRUN_PATH="\"$RUN\"" \
    -DMUOS_HOST_LAUNCH_FILE="\"$LAUNCH\"" \
    -DMUOS_HOST_SHUTDOWN_SCRIPT='"/tmp/shutdown.sh"' \
    -DMUOS_HOST_REBOOT_SCRIPT='"/tmp/reboot.sh"' \
    -I"$ROOT/upstream" -I"$ROOT/upstream/module" -I"$ROOT/upstream/common" \
    -I"$ROOT/upstream/vendor" -I"$ROOT/upstream/vendor/lvgl" \
    -I"$ROOT/upstream/retro/core" -I"$SDL2_INCLUDE" \
    "$ROOT/bridge/test_collection_create_qemu.c" -ldl -o "$TMP/collection-runner"
MUOS_HOST_LOG_FILE="$TMP/muos.log" TF_PANEL_W=640 TF_PANEL_H=480 timeout 30 "$QEMU" -cpu 74Kf -L "$SYSROOT" \
    "$TMP/collection-runner" "$TMP/muos-core.so"
