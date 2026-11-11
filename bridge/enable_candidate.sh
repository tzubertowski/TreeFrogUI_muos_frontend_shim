#!/usr/bin/env bash
set -euo pipefail
SCRIPT=${1:?usage: enable_candidate.sh /mounted/sd/path/to/zhijack.sh}
test -f "$SCRIPT"
BACKUP="$SCRIPT.muos_frontend_mvp.bak"
OLD='FROGUI_CORE=/mnt/sdcard/cubegm/cores/frogui_libretro.so'
NEW='FROGUI_CORE=/mnt/sdcard/cubegm/cores/muos_frontend_mvp_libretro.so'
[[ ! -e "$BACKUP" ]] || { echo "Backup already exists: $BACKUP" >&2; exit 1; }
[[ $(grep -Fxc "$OLD" "$SCRIPT") == 1 ]] || {
    echo "Expected exactly one stock FROGUI_CORE assignment in $SCRIPT" >&2; exit 1;
}
cp "$SCRIPT" "$BACKUP"
TMP="$SCRIPT.muos_frontend_mvp.tmp"
sed "s|^$OLD$|$NEW|" "$SCRIPT" > "$TMP"
mv "$TMP" "$SCRIPT"
[[ $(grep -Fxc "$NEW" "$SCRIPT") == 1 ]] || {
    cp "$BACKUP" "$SCRIPT"; echo "Activation verification failed; restored backup." >&2; exit 1;
}
echo "Enabled candidate; exact backup: $BACKUP"
