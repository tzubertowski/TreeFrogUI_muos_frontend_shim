#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "$0")/.." && pwd)
DEST=${1:?usage: install_candidate.sh /path/to/sdcard-mount}
test -d "$DEST"
test -f "$ROOT/out/install/cubegm/cores/muos_frontend_mvp_libretro.so" || {
    echo "Run bridge/check_core_qemu.sh first to build and verify the candidate." >&2
    exit 1
}
[[ ! -e "$DEST/cubegm/cores/muos_frontend_mvp_libretro.so" && ! -e "$DEST/cubegm/muos_frontend_mvp" ]] || {
    echo "Candidate paths already exist on the card; refusing to overwrite them." >&2
    exit 1
}
mkdir -p "$DEST/cubegm"
cp -R "$ROOT/out/install/cubegm/." "$DEST/cubegm/"
echo "Installed candidate under $DEST/cubegm. Remove only the MVP core and runtime folder to roll back."
