#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "$0")/.." && pwd)
TOOLCHAIN=${SF3000_TOOLCHAIN:-/home/tomaszz/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot}
CC=${MIPSEL_CC:-$TOOLCHAIN/opt/ext-toolchain/bin/mips-mti-linux-gnu-gcc}
SYSROOT=${MIPSEL_SYSROOT:-$TOOLCHAIN/mipsel-buildroot-linux-gnu/sysroot}
SDL2_INCLUDE=${SDL2_INCLUDE:-/home/tomaszz/sf3000-work/FroggyDuke/thirdparty/include}
OUT=${1:-$ROOT/out/install/cubegm/cores/muos_frontend_mvp_libretro.so}
OPT_PATH=${MUOS_OPT_PATH:-/mnt/sdcard/cubegm/muos_frontend_mvp/opt/}
RUN_PATH=${MUOS_RUN_PATH:-/mnt/sdcard/cubegm/muos_frontend_mvp/run/}
LAUNCH_FILE=${MUOS_HOST_LAUNCH_FILE:-/mnt/sdcard/cubegm/frogui_launch.txt}
SHUTDOWN_SCRIPT=${MUOS_HOST_SHUTDOWN_SCRIPT:-/mnt/sdcard/cubegm/shutdown.sh}
REBOOT_SCRIPT=${MUOS_HOST_REBOOT_SCRIPT:-/mnt/sdcard/cubegm/reboot.sh}
CORES_PATH=${MUOS_HOST_CORES_PATH:-/mnt/sdcard/cubegm/cores}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
HOST_TEST=()
if [[ ${MUOS_HOST_TEST:-0} == 1 ]]; then HOST_TEST=(-DMUOS_HOST_TEST); fi
test -x "$CC" && test -d "$SYSROOT"
mkdir -p "$(dirname -- "$OUT")" "$ROOT/out"
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
cat > "$TMP/exports.map" <<'EOF'
{
  global: retro_*;
  local: *;
};
EOF
sed -e 's/^#define LV_USE_GPU_SDL 1$/#define LV_USE_GPU_SDL 0/' \
    -e 's/^#define LV_USE_TINY_TTF 1$/#define LV_USE_TINY_TTF 0/' \
    -e 's/^#define LV_TICK_CUSTOM 1$/#define LV_TICK_CUSTOM 0/' \
    "$ROOT/upstream/vendor/lvgl/lv_conf.h" > "$TMP/lv_conf.h"
# shortcut: the MVP omits download/sysinfo services; include curl_api.c when either service is enabled.
mapfile -t COMMON < <(find "$ROOT/upstream/common" -name '*.c' \
    ! -path '*/platform/display.c' ! -path '*/platform/video.c' \
    ! -path '*/platform/usb_link.c' ! -path '*/runtime/crash.c' \
    ! -path '*/storage/curl_api.c' \
    ! -path '*/vendor/*' ! -path '*/ui/font.c' ! -path '*/ui/font_info.c' \
    ! -path '*/display/image.c' ! -path '*/ui/nav/cell_art.c' \
    ! -path '*/storage/download.c' ! -path '*/platform/sysinfo.c' \
    ! -path '*/compat/sdl_cursor.c' -print | sort)
mapfile -t LVGL < <(find "$ROOT/upstream/vendor/lvgl/src" -name '*.c' \
    ! -path '*/draw/sdl/*' ! -path '*/extra/libs/tiny_ttf/*' -print | sort)
mapfile -t PLUTOVG < <(find "$ROOT/upstream/vendor/plutosvg/plutovg/source" -name '*.c' -print | sort)
"$CC" -std=gnu11 -D_DEFAULT_SOURCE -O2 -w -mips32r2 -mhard-float -mfp32 -EL \
    --sysroot="$SYSROOT" -fPIC -fvisibility=hidden -G0 -ffunction-sections -fdata-sections \
    -DLV_CONF_INCLUDE_SIMPLE \
    -DTEST_IMAGE=0 \
    -DMUOS_HOST_ADAPTER \
    "${HOST_TEST[@]}" \
    -DOPT_PATH='"'"$OPT_PATH"'"' -DRUN_PATH='"'"$RUN_PATH"'"' \
    -DMUOS_HOST_LAUNCH_FILE='"'"$LAUNCH_FILE"'"' \
    -DMUOS_HOST_SHUTDOWN_SCRIPT='"'"$SHUTDOWN_SCRIPT"'"' \
    -DMUOS_HOST_REBOOT_SCRIPT='"'"$REBOOT_SCRIPT"'"' \
    -DMUOS_HOST_CORES_PATH='"'"$CORES_PATH"'"' \
    -I"$TMP" -I"$ROOT/bridge" -I"$ROOT/upstream" -I"$ROOT/upstream/module" \
    -I"$ROOT/upstream/vendor" -I"$ROOT/upstream/vendor/lvgl" \
    -I"$ROOT/upstream/vendor/plutosvg/source" \
    -I"$ROOT/upstream/vendor/plutosvg/plutovg/include" \
    -I"$ROOT/upstream/common" -I"$ROOT/upstream/retro/core" -I"$SDL2_INCLUDE" \
    "$ROOT/bridge/muos_frontend_libretro.c" "$ROOT/bridge/muos_host_supervisor.c" \
    "$ROOT/bridge/muos_host_diag.c" \
    "$ROOT/bridge/muos_host_services_mvp.c" \
    "$ROOT/bridge/muos_image_host.c" \
    "$ROOT/bridge/retro_lvgl_bridge.c" \
    "$ROOT/bridge/bridge_logic.c" "$ROOT/upstream/module/muxlaunch.c" \
    "$ROOT/upstream/module/muxplore.c" "$ROOT/upstream/module/muxcollect.c" \
    "$ROOT/upstream/module/muxhistory.c" "$ROOT/upstream/module/muxapp.c" \
    "$ROOT/upstream/module/muxinfo.c" "$ROOT/upstream/module/muxconfig.c" \
    "$ROOT/upstream/module/muxsearch.c" "$ROOT/upstream/module/muxassign.c" \
    "$ROOT/upstream/module/muxtag.c" "$ROOT/upstream/module/muxoption.c" \
    "$ROOT/upstream/module/muxorder.c" \
    "$ROOT/upstream/module/ui/ui_muxlaunch.c" "$ROOT/upstream/module/ui/ui_muxcollect.c" \
    "$ROOT/upstream/module/ui/ui_muxhistory.c" "$ROOT/upstream/module/ui/ui_muxinfo.c" \
    "$ROOT/upstream/module/ui/ui_muxconfig.c" "$ROOT/upstream/module/ui/ui_muxsearch.c" \
    "$ROOT/upstream/module/ui/ui_muxoption.c" "$ROOT/upstream/module/muxshare.c" \
    "${COMMON[@]}" "${LVGL[@]}" "$ROOT/upstream/vendor/json/json.c" \
    "$ROOT/upstream/vendor/mini/mini.c" "$ROOT/upstream/vendor/plutosvg/source/plutosvg.c" \
    "${PLUTOVG[@]}" -DPLUTOSVG_BUILD -shared \
    -Wl,--gc-sections,--no-undefined,--version-script="$TMP/exports.map" \
    -L"$SYSROOT/usr/lib" -lm -ldl -lc -lpthread -o "$OUT"
echo "Built $OUT"
