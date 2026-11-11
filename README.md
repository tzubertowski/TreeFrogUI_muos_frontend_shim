# muOS frontend / FrogUI bridge MVP

This keeps the upstream MustardOS frontend source in `upstream/` and adds a
small LVGL 8.4 to libretro callback adapter in `bridge/`. The adapter converts
LVGL flushes to the RGB565 video callback and maps FrogUI/libretro input to
LVGL keypad events. Upstream has a separate per-frame input API that dispatches
through genuine muOS input handlers.

## Build and QEMU test

To compile the libretro core without running QEMU:

```sh
SF3000_TOOLCHAIN=/path/to/sdk \
SDL2_INCLUDE=/path/to/sdl2/include \
bash bridge/build_libretro.sh out/muos_frontend_mvp_libretro.so
```

The full MVP check cross-builds the adapter/core, runs it under MIPS QEMU, and
stages the candidate at `out/install/cubegm/`:

```sh
bash bridge/check_core_qemu.sh
```

It needs the SF3000 MIPS Buildroot SDK, `qemu-mipsel-static`, FrogUI's built
core directory and mapping source, an SDL2-compatible header directory, and
the MinUIfied Horizontal theme archive. The archive is excluded from version
control; pass its path using `MUOS_THEME_ARCHIVE`. Default paths match the
sibling-project layout in this workspace. Override them when needed:

```sh
SF3000_TOOLCHAIN=/path/to/sdk \
FROGUI_CORES_PATH=/path/to/frogui/cores \
FROGUI_MAPPING_SOURCE=/path/to/frogui/frogui_libretro.c \
SDL2_INCLUDE=/path/to/sdl2/include \
QEMU_MIPSEL=/path/to/qemu-mipsel-static \
MUOS_THEME_ARCHIVE=/path/to/MinUIfied-Horizontal.muxthm \
bash bridge/check_core_qemu.sh
```

`MIPSEL_CC` and `MIPSEL_SYSROOT` can override the SDK compiler and sysroot
separately. `bridge/build_libretro.sh /path/to/output.so` builds only the
shared core. The QEMU script checks the launcher layout, screen routes, core
assignment/handoff, power actions, text/font rendering, resolution, and
collection creation before staging the device candidate.

## Build the bridge object

```sh
make -C bridge
make -C bridge check
```

## MIPS QEMU check

Run `bash bridge/check_qemu.sh` to cross-build the actual adapter and vendored
LVGL for the SF3000 MIPS32r2 ABI, then execute them under `qemu-mipsel-static`.
It checks display registration and resolution limits, a real LVGL screen refresh
through the adapter's RGB565 video callback, and the libretro keypad polling and
button mapping path. It also directly invokes the registered LVGL flush callback
with a partial area to verify RGB565 updates preserve pixels outside that area.
The injected FrogUI callback path is checked for all four directions, A, B, and
release, including that it bypasses libretro polling.
The actual upstream `init_display_host()` is run with no display (failure) and
with the bridge display (success); the check verifies muOS resolution metadata,
SVG initialization hook, and that muOS stores the registered LVGL refresh timer.

The script compiles the real upstream `common/platform/input.c` and calls its
`mux_input_host_begin/step/end` APIs under QEMU. It exercises key press, hold,
release, combo press/repeat/release, duplicate begin rejection, mask bounds, and
the injected clock. Its test file supplies only environment hooks (clock,
config/device globals, board identity, and video-preview state); input mapping,
repeat, combo, and dispatch logic are upstream code. Function-section garbage
collection avoids linking unused SDL paths; this is not a full SDL or muOS build.
For `init_display_host`, the real `init.c` object is linked with unused runtime
symbols ignored by the linker; the executed host-init path supplies only the
device global and SVG hook required by that function.

The final check also builds and starts the real upstream `muxlaunch` screen
setup under QEMU. It uses `init_runtime_host()`, the extracted host entry for
upstream menu construction, and upstream per-frame input dispatch. The test
checks visible RGB565 framebuffer output and verifies that d-pad-down changes
the selected menu item. Its temporary config/theme fixture uses LVGL's bundled
unscii font and no image assets; the harness stubs SDL-only display, wallpaper,
network, clock, font-loader, and video-preview services. The menu, theme logic,
widgets, and input handlers are upstream code.

The LVGL bridge test uses a temporary config with SDL GPU, TinyTTF, and stdio
filesystem support disabled and LVGL's test tick enabled; upstream config stays unchanged.
`SF3000_TOOLCHAIN`, `MIPSEL_CC`, `MIPSEL_SYSROOT`, `QEMU_MIPSEL`, and
`SDL2_INCLUDE` can override the defaults.

The QEMU fixture validates the launcher's screen construction and one input
navigation step, not the full SDL event loop or services that require muOS
hardware. QEMU does not validate hardware video, timing, or button wiring.

## Input integration contract

Pass a `muos_pressed_cb` that reads `input_is_pressed()` for the corresponding
`FrogButton` values (`FROG_BTN_UP`, `FROG_BTN_DOWN`, `FROG_BTN_LEFT`,
`FROG_BTN_RIGHT`, `FROG_BTN_A`, `FROG_BTN_B`). Call
`input_init()` before attaching the bridge. Call `input_update()` once per
outer FrogUI frame, before `lv_timer_handler()`; do not call it from the LVGL
read callback, since that would break FrogUI's edge/debounce state. With an
injected callback, the bridge does not poll libretro input. If it is NULL, the
bridge uses libretro's input poll/state callbacks instead.

After `lv_init()` and bridge attachment, call `init_runtime_host()`. It loads
the real muOS device, config and language data, binds resolution metadata and
the refresh timer to the registered LVGL display, then loads the active theme.
The host must provide the normal muOS config, device and theme paths. For
the real launcher screen, call `muxlaunch_host_start()` and later
`muxlaunch_host_end()`. These use the upstream menu construction and input
handlers while leaving frame timing to the host. For custom per-frame upstream
navigation, call `mux_input_host_begin(&opts)`, then
`mux_input_host_step(mask)` once per frame with a `BIT(mux_input_*)` mask, and
call `mux_input_host_end()` when leaving the menu. This reuses the real muOS
press, release, repeat, combo, and navigation handlers without polling SDL or
opening devices. The existing `muxlaunch_main()` and `mux_input_task()` remain
the SDL blocking path.

muOS configures LVGL for 32-bit color, so the bridge converts each flush to
FrogUI's RGB565 format.

The upstream `build.sh` supports ARM and x86 targets, but not this device's
MIPS toolchain. A native build also requires SDL2, SDL2_mixer, SDL2_ttf,
SDL2_image, and related development libraries, which are absent in this
environment. The upstream frontend is GPL-3.0; any linked/modified binary
will need corresponding source and license handling.

Upstream is a submodule at `upstream/`, pinned to the host-adapter branch in
the [TreeFrogUI MuOS frontend fork](https://github.com/tzubertowski/TreeFrogUI_MuOS_frontend/tree/feature/libretro-host-adapter-mvp).
No FrogUI source was changed.
