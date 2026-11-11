#!/usr/bin/env bash
set -euo pipefail
SCRIPT=${1:?usage: rollback_candidate.sh /mounted/sd/path/to/zhijack.sh}
BACKUP="$SCRIPT.muos_frontend_mvp.bak"
NEW='FROGUI_CORE=/mnt/sdcard/cubegm/cores/muos_frontend_mvp_libretro.so'
OLD='FROGUI_CORE=/mnt/sdcard/cubegm/cores/frogui_libretro.so'
test -f "$SCRIPT" && test -f "$BACKUP" || { echo "Launcher or backup missing." >&2; exit 1; }
[[ $(grep -Fxc "$NEW" "$SCRIPT") == 1 ]] || {
    echo "Launcher no longer has the expected candidate assignment; refusing restore." >&2; exit 1;
}
[[ $(grep -Fxc "$OLD" "$BACKUP") == 1 ]] || {
    echo "Backup does not contain the expected stock core assignment; refusing restore." >&2; exit 1;
}
cp "$BACKUP" "$SCRIPT.restore.tmp"
mv "$SCRIPT.restore.tmp" "$SCRIPT"
echo "Restored exact launcher backup from $BACKUP"
