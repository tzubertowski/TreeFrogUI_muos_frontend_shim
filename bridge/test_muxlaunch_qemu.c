#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "lvgl.h"
#include "retro_lvgl_bridge.h"
#include "common/base/options.h"
#include "common/config/config.h"
#include "common/platform/device.h"
#include "common/runtime/init.h"
#include "common/platform/input.h"
#include "module/muxshare.h"

static uint16_t frame[320 * 240];
static uint16_t initial_frame[320 * 240];
static unsigned frames;

static void video(const void *data, unsigned width, unsigned height, size_t pitch) {
    assert(width == 320 && height == 240 && pitch == 640);
    for (size_t i = 0; i < 320 * 240; ++i) frame[i] = ((const uint16_t *)data)[i];
    ++frames;
}

static void svg_init(void) {}

static int make_fixture(void) {
    char path[MAX_BUFFER_SIZE];
    snprintf(path, sizeof(path), "%smux", CONF_DEVICE_PATH);
    mkdir(OPT_PATH, 0755);
    mkdir(OPT_PATH "device", 0755);
    mkdir(OPT_PATH "device/config", 0755);
    mkdir(OPT_PATH "device/config/mux", 0755);
    mkdir(OPT_PATH "config", 0755);
    mkdir(OPT_PATH "config/visual", 0755);
    mkdir(OPT_PATH "config/settings", 0755);
    mkdir(OPT_PATH "config/settings/general", 0755);
    snprintf(path, sizeof(path), "%sblackfade", CONF_CONFIG_PATH "visual/");
    FILE *f = fopen(path, "w"); if (!f) return 0; fputs("0\n", f); fclose(f);
    snprintf(path, sizeof(path), "%sorientation", CONF_CONFIG_PATH "settings/general/");
    f = fopen(path, "w"); if (!f) return 0; fputs("0\n", f); fclose(f);
    snprintf(path, sizeof(path), "%swidth", CONF_DEVICE_PATH "mux/");
    f = fopen(path, "w"); if (!f) return 0; fputs("320\n", f); fclose(f);
    snprintf(path, sizeof(path), "%sheight", CONF_DEVICE_PATH "mux/");
    f = fopen(path, "w"); if (!f) return 0; fputs("240\n", f); fclose(f);
    mkdir(OPT_PATH "share", 0755);
    mkdir(OPT_PATH "share/theme", 0755);
    mkdir(INTERNAL_THEME, 0755);
    f = fopen(INTERNAL_THEME "/version.txt", "w"); if (!f) return 0;
    fputs("Edge\n", f); fclose(f);
    mkdir(OPT_PATH "share/font", 0755);
    mkdir(INTERNAL_FONTS, 0755);
    return 1;
}

static unsigned nonblack_pixels(void) {
    unsigned count = 0;
    for (size_t i = 0; i < 320 * 240; ++i) count += frame[i] != 0;
    return count;
}

int main(void) {
    assert(make_fixture());
    lv_init();
    assert(muos_retro_lvgl_attach(320, 240, video, NULL, NULL, NULL));
    assert(init_runtime_host());
    assert(lv_font_unscii_8.get_glyph_dsc != NULL);
    assert(device.mux.width == 320 && device.mux.height == 240);
    assert(muxlaunch_host_start());
    lv_timer_handler();
    lv_refr_now(NULL);
    assert(frames && nonblack_pixels() > 1000);
    memcpy(initial_frame, frame, sizeof(frame));

    const int before = current_item_index;
    mux_input_host_step(UINT64_C(1) << mux_input_dpad_down);
    mux_input_host_step(0);
    lv_timer_handler();
    lv_refr_now(NULL);
    assert(current_item_index != before);
    assert(frames > 1 && nonblack_pixels() > 1000);
    unsigned changed_pixels = 0;
    for (size_t i = 0; i < 320 * 240; ++i) changed_pixels += frame[i] != initial_frame[i];
    assert(changed_pixels > 0);
    muxlaunch_host_end();
    return 0;
}
