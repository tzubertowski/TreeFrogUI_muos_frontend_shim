#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "$0")/.." && pwd)
TOOLCHAIN=${SF3000_TOOLCHAIN:-/home/tomaszz/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot}
CC=${MIPSEL_CC:-$TOOLCHAIN/opt/ext-toolchain/bin/mips-mti-linux-gnu-gcc}
SYSROOT=${MIPSEL_SYSROOT:-$TOOLCHAIN/mipsel-buildroot-linux-gnu/sysroot}
QEMU=${QEMU_MIPSEL:-qemu-mipsel-static}
TMP=$(mktemp -d)
if [[ ${KEEP_TMP:-0} == 1 ]]; then
    echo "Keeping QEMU build at $TMP"
else
    trap 'rm -rf "$TMP"' EXIT
fi
OPT="$TMP/opt/muos/"
RUN="$TMP/run/muos/"
LAUNCH="$TMP/frogui_launch.txt"
SHUTDOWN_SCRIPT=/mnt/sdcard/cubegm/shutdown.sh
REBOOT_SCRIPT=/mnt/sdcard/cubegm/reboot.sh
CORES=${FROGUI_CORES_PATH:-$ROOT/../sf3000_treefrogui/release/latest/release/cubegm/cores}
FROGUI_MAPPING_SOURCE=${FROGUI_MAPPING_SOURCE:-$ROOT/../sf3000_treefrogui/frogui/frogui_libretro.c}
SDL2_INCLUDE=${SDL2_INCLUDE:-/home/tomaszz/sf3000-work/FroggyDuke/thirdparty/include}
THEME_ARCHIVE=${MUOS_THEME_ARCHIVE:-$ROOT/assets/MinUIfied-Horizontal.muxthm}
test -s "$CORES/fceumm_libretro.so"
test -s "$FROGUI_MAPPING_SOURCE"
test -s "$THEME_ARCHIVE" || { echo "Set MUOS_THEME_ARCHIVE to MinUIfied Horizontal .muxthm" >&2; exit 1; }
COREDB="$TMP/muos_coredb"
python3 "$ROOT/bridge/generate_frogui_coredb.py" \
    "$FROGUI_MAPPING_SOURCE" "$CORES" "$COREDB"
python3 - "$COREDB/core/treefrogui" <<'PY'
import json, pathlib, sys
catalogues = {json.loads(p.read_text())["catalogue"] for p in pathlib.Path(sys.argv[1]).glob("*.json")}
assert {"Nintendo NES-Famicom", "Nintendo SNES-SFC", "Nintendo Game Boy Advance"} <= catalogues
PY
mkdir -p "$OPT" "$RUN/storage/theme/MinUIfied.-.Horizontal" "$RUN/storage/theme/override" \
    "$RUN/storage/info/manifest" "$OPT/share/info/manifest/core"
mkdir -p "$TMP/ROMS/FC"
unzip -oq "$THEME_ARCHIVE" -d "$RUN/storage/theme/MinUIfied.-.Horizontal"
cp "$ROOT/assets/muos/muxlaunch.ini" "$RUN/storage/theme/override/muxlaunch.ini"
cp "$COREDB/assign.json" "$RUN/storage/info/manifest/assign.json"
cp -R "$COREDB/core/treefrogui" "$OPT/share/info/manifest/core/"
MUOS_OPT_PATH=$OPT MUOS_RUN_PATH=$RUN MUOS_HOST_LAUNCH_FILE=$LAUNCH MUOS_HOST_TEST=1 \
MUOS_HOST_SHUTDOWN_SCRIPT=$SHUTDOWN_SCRIPT MUOS_HOST_REBOOT_SCRIPT=$REBOOT_SCRIPT \
MUOS_HOST_CORES_PATH=$CORES \
    "$ROOT/bridge/build_libretro.sh" "$TMP/muos-core.so"
"$CC" -std=gnu11 -mips32r2 -mhard-float -EL --sysroot="$SYSROOT" \
    -DOPT_PATH="\"$OPT\"" -DRUN_PATH="\"$RUN\"" \
    -DMUOS_HOST_LAUNCH_FILE="\"$LAUNCH\"" \
    -DMUOS_HOST_SHUTDOWN_SCRIPT="\"$SHUTDOWN_SCRIPT\"" \
    -DMUOS_HOST_REBOOT_SCRIPT="\"$REBOOT_SCRIPT\"" \
    -DMUOS_HOST_CORES_PATH="\"$CORES\"" \
    -I"$ROOT/upstream" -I"$ROOT/upstream/module" -I"$ROOT/upstream/common" \
    -I"$ROOT/upstream/vendor" -I"$ROOT/upstream/vendor/lvgl" \
    -I"$ROOT/upstream/retro/core" -I"$SDL2_INCLUDE" \
    "$ROOT/bridge/test_core_dlopen_qemu.c" -ldl -o "$TMP/core-runner"
MUOS_HOST_ASSIGN_DIR="$TMP/ROMS/FC" MUOS_HOST_LOG_FILE="$TMP/muos.log" \
    TF_PANEL_W=640 TF_PANEL_H=480 timeout 30 "$QEMU" -cpu 74Kf -L "$SYSROOT" \
    "$TMP/core-runner" "$TMP/muos-core.so"
grep -q 'init_runtime_host=1' "$TMP/muos.log" || { cat "$TMP/muos.log"; exit 1; }
grep -q 'TEST_IMAGE=0' "$TMP/muos.log" || { cat "$TMP/muos.log"; exit 1; }
grep -q 'theme_layout module=muxlaunch menu_style=2 active=1 grid=1:2x4 nav=0/0 .*cell_text=255/255 label_text=255' "$TMP/muos.log" || {
    cat "$TMP/muos.log"
    exit 1
}
if grep -q 'image_decoded path=.*/640x480/image/static/muxlaunch/explore.png' "$TMP/muos.log"; then
    cat "$TMP/muos.log"
    exit 1
fi
grep -q 'screen started module=muxplore' "$TMP/muos.log" || { cat "$TMP/muos.log"; exit 1; }
for module in muxlaunch muxcollect muxhistory muxapp muxinfo muxconfig muxsearch muxassign muxtag muxoption muxorder; do
    grep -q "screen started module=$module" "$TMP/muos.log" || { cat "$TMP/muos.log"; exit 1; }
done
grep -q 'theme_font loaded .*line_height=[1-9][0-9]*' "$TMP/muos.log" || { cat "$TMP/muos.log"; exit 1; }

"$CC" -std=gnu11 -mips32r2 -mhard-float -EL --sysroot="$SYSROOT" \
    -I"$ROOT/upstream/retro/core" "$ROOT/bridge/test_frogui_core_qemu.c" -ldl -o "$TMP/frogui-core-runner"
timeout 30 "$QEMU" -cpu 74Kf -L "$SYSROOT" \
    "$TMP/frogui-core-runner" "$LAUNCH"

CANDIDATE="$ROOT/out/install/cubegm"
"$ROOT/bridge/build_libretro.sh" "$CANDIDATE/cores/muos_frontend_mvp_libretro.so"
install -m 0755 "$ROOT/bridge/reboot.sh" "$CANDIDATE/reboot.sh"
OPT_CANDIDATE="$CANDIDATE/muos_frontend_mvp/opt"
mkdir -p "$OPT_CANDIDATE/device/config/mux" \
    "$OPT_CANDIDATE/config/visual" "$OPT_CANDIDATE/config/settings/general" \
    "$OPT_CANDIDATE/config/settings/theme" \
    "$OPT_CANDIDATE/config/theme" \
    "$OPT_CANDIDATE/share/theme/MustardOS" "$OPT_CANDIDATE/share/font/internal"
printf '640\n' > "$OPT_CANDIDATE/device/config/mux/width"
printf '480\n' > "$OPT_CANDIDATE/device/config/mux/height"
printf '0\n' > "$OPT_CANDIDATE/config/visual/blackfade"
printf '0\n' > "$OPT_CANDIDATE/config/settings/general/orientation"
printf '0\n' > "$OPT_CANDIDATE/config/settings/theme/menu_style"
printf 'MinUIfied.-.Horizontal\n' > "$OPT_CANDIDATE/config/theme/active"
printf 'Edge\n' > "$OPT_CANDIDATE/share/theme/MustardOS/version.txt"
mkdir -p "$CANDIDATE/muos_frontend_mvp/run/storage/theme/MinUIfied.-.Horizontal" \
    "$CANDIDATE/muos_frontend_mvp/run/storage/theme/override" \
    "$CANDIDATE/muos_frontend_mvp/run/storage/info/manifest" \
    "$OPT_CANDIDATE/share/info/manifest/core"
unzip -oq "$THEME_ARCHIVE" -d \
    "$CANDIDATE/muos_frontend_mvp/run/storage/theme/MinUIfied.-.Horizontal"
cp "$ROOT/assets/muos/muxlaunch.ini" \
    "$CANDIDATE/muos_frontend_mvp/run/storage/theme/override/muxlaunch.ini"
cp "$COREDB/assign.json" \
    "$CANDIDATE/muos_frontend_mvp/run/storage/info/manifest/assign.json"
cp -R "$COREDB/core/treefrogui" "$OPT_CANDIDATE/share/info/manifest/core/"
echo "Candidate staged at $CANDIDATE"
