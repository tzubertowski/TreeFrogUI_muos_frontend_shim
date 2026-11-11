#!/bin/bash
set -euo pipefail

ROOT=$(cd -- "$(dirname -- "$0")/.." && pwd)
TOOLCHAIN=${SF3000_TOOLCHAIN:-/home/tomaszz/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot}
CC=${MIPSEL_CC:-$TOOLCHAIN/opt/ext-toolchain/bin/mips-mti-linux-gnu-gcc}
SYSROOT=${MIPSEL_SYSROOT:-$TOOLCHAIN/mipsel-buildroot-linux-gnu/sysroot}
QEMU=${QEMU_MIPSEL:-qemu-mipsel-static}
SDL2_INCLUDE=${SDL2_INCLUDE:-/home/tomaszz/sf3000-work/FroggyDuke/thirdparty/include}
TMP=$(mktemp -d)
if [[ ${KEEP_TMP:-0} == 1 ]]; then
	echo "Keeping QEMU build at $TMP"
else
	trap 'rm -rf "$TMP"' EXIT
fi

command -v "$QEMU" >/dev/null
test -x "$CC"
test -d "$SYSROOT"
OPT_FIXTURE=$TMP/opt/muos
RUN_FIXTURE=$TMP/run/muos
mkdir -p "$OPT_FIXTURE" "$RUN_FIXTURE"
mkdir -p "$TMP/mojibake"
cat > "$TMP/mojibake/mojibake.h" <<'EOF'
#include <stddef.h>
typedef int mjb_status;
#define MJB_ENC_UTF_8 1
#define MJB_CASE_CASEFOLD 1
#define MJB_STATUS_OK 0
#define MJB_COLLATION_SHIFTED 1
#define MJB_COLLATION_TERTIARY 1
mjb_status mjb_map_case_into(const char *, size_t, int, int, int, char *, size_t *);
mjb_status mjb_collation_compare(const char *, size_t, int, const char *, size_t, int, int, int, int *);
EOF

sed -e 's/^#define LV_USE_GPU_SDL 1$/#define LV_USE_GPU_SDL 0/' \
	-e 's/^#define LV_USE_FS_STDIO 1$/#define LV_USE_FS_STDIO 0/' \
	-e 's/^#define LV_USE_TINY_TTF 1$/#define LV_USE_TINY_TTF 0/' \
	-e 's/^#define LV_TICK_CUSTOM 1$/#define LV_TICK_CUSTOM 0/' \
	"$ROOT/upstream/vendor/lvgl/lv_conf.h" > "$TMP/lv_conf.h"
mapfile -t COMMON_SOURCES < <(find "$ROOT/upstream/common" -name '*.c' \
	! -path '*/platform/display.c' \
	! -path '*/platform/video.c' ! -path '*/platform/usb_link.c' \
	! -path '*/runtime/crash.c' \
	! -path '*/vendor/*' ! -path '*/ui/font.c' ! -path '*/ui/font_info.c' \
	! -path '*/ui/image.c' ! -path '*/display/image.c' ! -path '*/ui/nav/cell_art.c' \
	! -path '*/storage/download.c' ! -path '*/platform/sysinfo.c' \
	! -path '*/compat/sdl_cursor.c' -print | sort)
mapfile -t LVGL_SOURCES < <(find "$ROOT/upstream/vendor/lvgl/src" -name '*.c' \
	! -path '*/draw/sdl/*' ! -path '*/extra/libs/tiny_ttf/*' -print | sort)

"$CC" -std=gnu11 -D_DEFAULT_SOURCE -O1 -g -w -mips32r2 -mhard-float -EL \
	-ffunction-sections -fdata-sections -DLV_CONF_INCLUDE_SIMPLE \
	-DOPT_PATH="\"$OPT_FIXTURE/\"" -DRUN_PATH="\"$RUN_FIXTURE/\"" \
	-I"$TMP" -I"$ROOT/bridge" -I"$ROOT/upstream" -I"$ROOT/upstream/module" \
	-I"$ROOT/upstream/vendor" -I"$ROOT/upstream/vendor/lvgl" \
	-I"$ROOT/upstream/vendor/plutosvg/source" \
	-I"$ROOT/upstream/vendor/plutosvg/plutovg/include" \
	-I"$ROOT/upstream/common" -I"$ROOT/upstream/retro/core" -I"$SDL2_INCLUDE" \
	"$ROOT/bridge/test_muxlaunch_qemu.c" "$ROOT/bridge/retro_lvgl_bridge.c" \
	"$ROOT/bridge/muos_host_diag.c" "$ROOT/bridge/bridge_logic.c" "$ROOT/bridge/test_muos_host_services.c" \
	"$ROOT/upstream/module/muxlaunch.c" \
	"$ROOT/upstream/module/ui/ui_muxlaunch.c" "$ROOT/upstream/module/muxshare.c" \
	"${COMMON_SOURCES[@]}" "${LVGL_SOURCES[@]}" \
	-Wl,--gc-sections,--unresolved-symbols=ignore-all -lm -ldl -o "$TMP/muxlaunch-qemu"

	timeout 30 "$QEMU" -strace -cpu 74Kf -L "$SYSROOT" "$TMP/muxlaunch-qemu" 2>"$TMP/qemu.trace" || {
	tail -40 "$TMP/qemu.trace"
	false
}
echo "QEMU MIPS check passed (upstream muxlaunch screen, framebuffer, and host input)."
