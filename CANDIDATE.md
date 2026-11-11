# Device test candidate

Run `bash bridge/check_core_qemu.sh`. It builds the MIPS libretro core, loads
that core with `dlopen()` under QEMU, runs MinUIfied Horizontal at 640x480,
checks physical D-pad orientation and visible text, exercises the MuOS folder
assignment generated from FrogUI's mapping, and verifies both default and
alternate core handoffs. It also uses test-only route injection to start and
return from linked screens, checks power handoffs, and stages the payload in
`out/install/cubegm/`.

The core path is `out/install/cubegm/cores/muos_frontend_mvp_libretro.so`.
Its isolated config and theme path is
`out/install/cubegm/muos_frontend_mvp/opt/`; MinUIfied Horizontal is unpacked
under the matching runtime path
`/mnt/sdcard/cubegm/muos_frontend_mvp/run/storage/theme/MinUIfied.-.Horizontal/`.
The runtime includes FrogUI-derived `assign.json` and MuOS CoreDB entries under
the adapter's isolated paths. MuOS then creates its normal
`share/info/content/<system>/core.cfg` files, and the Assign menu can change
among installed FrogUI cores for systems with alternatives.
The core reports and renders the `TF_PANEL_W` / `TF_PANEL_H` values from the
device launcher, with 320x240 as the host fallback.

After the SD card is mounted at its actual mountpoint, install with
`bash bridge/install_candidate.sh /path/to/mountpoint`. The helper only copies
the `cubegm` candidate subtree. To opt into a device boot test, pass the mounted
`zhijack.sh` file to `bridge/enable_candidate.sh`; it backs up that exact file
once and changes only the `FROGUI_CORE` assignment. Run
`bridge/rollback_candidate.sh` with the same script path to restore the exact
backup. On the R36HD card, that opt-in is already active and the original
launcher backup is `cubegm/zhijack.sh.muos_frontend_mvp.bak`.

The host supervisor watches muOS's normal `RUN_PATH/action` request. For
`shutdown` or `reboot`, it writes FrogUI's standalone launch contract and asks
the libretro frontend to exit so the existing `zhijack.sh` supervisor runs the
corresponding script. No power-action special case was added to upstream muOS.

This is still an MVP. The upstream top-level menu and six screen constructors
are real, but the host currently lacks the nested MuOS modules those screens
can request, plus game/core and app launch handoff. Passcode-protected launch
is denied by the host stub. Video previews, audio, and SDL video overlays are
disabled; GIFs show their first frame. Runtime diagnostics are written to
`/mnt/sdcard/cubegm/logs/muos_frontend_mvp.log` and the prior run is kept as
`.prev`.

The check needs an SF3000 MIPS Buildroot SDK, `qemu-mipsel-static`, built
FrogUI cores and its `frogui_libretro.c` mapping, SDL2-compatible headers, and
MinUIfied Horizontal theme archive (set `MUOS_THEME_ARCHIVE`). The scripts
accept `SF3000_TOOLCHAIN`,
`MIPSEL_CC`, `MIPSEL_SYSROOT`, `QEMU_MIPSEL`, `FROGUI_CORES_PATH`,
`FROGUI_MAPPING_SOURCE`, and `SDL2_INCLUDE` overrides; see `README.md` for an
example. Generated outputs live under `out/`.
