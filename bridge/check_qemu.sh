#!/bin/bash
set -euo pipefail

ROOT=$(cd -- "$(dirname -- "$0")/.." && pwd)
TOOLCHAIN=${SF3000_TOOLCHAIN:-/home/tomaszz/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot}
CC=${MIPSEL_CC:-$TOOLCHAIN/opt/ext-toolchain/bin/mips-mti-linux-gnu-gcc}
SYSROOT=${MIPSEL_SYSROOT:-$TOOLCHAIN/mipsel-buildroot-linux-gnu/sysroot}
QEMU=${QEMU_MIPSEL:-qemu-mipsel-static}
SDL2_INCLUDE=${SDL2_INCLUDE:-/home/tomaszz/sf3000-work/FroggyDuke/thirdparty/include}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

command -v "$QEMU" >/dev/null
test -x "$CC"
test -d "$SYSROOT"
sed -e 's/^#define LV_USE_GPU_SDL 1$/#define LV_USE_GPU_SDL 0/' \
	-e 's/^#define LV_USE_TINY_TTF 1$/#define LV_USE_TINY_TTF 0/' \
	-e 's/^#define LV_USE_FS_STDIO 1$/#define LV_USE_FS_STDIO 0/' \
	-e 's/^#define LV_TICK_CUSTOM 1$/#define LV_TICK_CUSTOM 0/' \
	"$ROOT/upstream/vendor/lvgl/lv_conf.h" > "$TMP/lv_conf.h"
mapfile -t LVGL_SOURCES < <(find "$ROOT/upstream/vendor/lvgl/src" -name '*.c' -print | sort)


"$CC" -std=gnu11 -D_DEFAULT_SOURCE -O1 -g -mips32r2 -mhard-float -EL \
	-ffunction-sections -fdata-sections -DLV_CONF_INCLUDE_SIMPLE \
	-I"$ROOT/upstream" -I"$ROOT/upstream/vendor" -I"$ROOT/upstream/vendor/lvgl" \
	-I"$ROOT/upstream/common" -I"$ROOT/upstream/retro/core" -I"$SDL2_INCLUDE" \
	-c "$ROOT/upstream/common/runtime/init.c" -o "$TMP/init-host.o"


"$CC" -std=gnu11 -D_DEFAULT_SOURCE -O1 -g -mips32r2 -mhard-float -EL \
	-ffunction-sections -fdata-sections -DLV_CONF_INCLUDE_SIMPLE \
	-I"$TMP" -I"$ROOT/bridge" -I"$ROOT/upstream" \
	-I"$ROOT/upstream/vendor" -I"$ROOT/upstream/vendor/lvgl" \
	-I"$ROOT/upstream/vendor/lvgl/src/core" \
	-I"$ROOT/upstream/common" -I"$ROOT/upstream/retro/core" -I"$SDL2_INCLUDE" \
	"$ROOT/bridge/test_bridge_qemu.c" "$ROOT/bridge/retro_lvgl_bridge.c" \
	"$ROOT/bridge/muos_host_diag.c" "$ROOT/bridge/bridge_logic.c" "$TMP/init-host.o" \
	"${LVGL_SOURCES[@]}" \
	-Wl,--gc-sections,--unresolved-symbols=ignore-all -o "$TMP/bridge-qemu"

"$QEMU" -cpu 74Kf -L "$SYSROOT" "$TMP/bridge-qemu" libretro
"$QEMU" -cpu 74Kf -L "$SYSROOT" "$TMP/bridge-qemu" frogui

"$CC" -std=gnu11 -D_DEFAULT_SOURCE -O1 -g -mips32r2 -mhard-float -EL \
	-ffunction-sections -fdata-sections -I"$ROOT/upstream" \
	-I"$ROOT/upstream/vendor" -I"$ROOT/upstream/vendor/lvgl" \
	-I"$ROOT/upstream/common" -I"$ROOT/upstream/retro/core" -I"$SDL2_INCLUDE" \
	"$ROOT/bridge/test_mux_input_qemu.c" "$ROOT/upstream/common/platform/input.c" \
	-Wl,--gc-sections -o "$TMP/mux-input-qemu"
"$QEMU" -cpu 74Kf -L "$SYSROOT" "$TMP/mux-input-qemu"
echo "QEMU MIPS checks passed (bridge callbacks, upstream input APIs, and init_display_host)."
bash "$ROOT/bridge/check_muxlaunch_qemu.sh"
