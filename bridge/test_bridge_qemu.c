#include <assert.h>
#include <stdint.h>
#include "lvgl.h"
#include "lv_refr.h"
#include "lv_indev.h"
#include "retro_lvgl_bridge.h"
#include "common/platform/device.h"
#include "common/runtime/init.h"

struct mux_device device;
static unsigned svg_inits;
static unsigned video_frames, polls, state_reads, frog_reads;
static int retro_pressed = -1;
static muos_button_t frog_pressed = MUOS_BTN_COUNT;
static uint16_t frame[4 * 4];

void svg_init(void) { ++svg_inits; }

static void video(const void *data, unsigned width, unsigned height, size_t pitch) {
    assert(width == 4 && height == 4 && pitch == 8);
    const uint16_t *pixels = data;
    for (unsigned i = 0; i < 16; ++i) frame[i] = pixels[i];
    ++video_frames;
}

static void poll(void) { ++polls; }

static int16_t input_state(unsigned port, unsigned device, unsigned index, unsigned id) {
    assert(port == 0 && device == RETRO_DEVICE_JOYPAD && index == 0);
    assert(id == RETRO_DEVICE_ID_JOYPAD_UP || id == RETRO_DEVICE_ID_JOYPAD_DOWN ||
           id == RETRO_DEVICE_ID_JOYPAD_LEFT || id == RETRO_DEVICE_ID_JOYPAD_RIGHT ||
           id == RETRO_DEVICE_ID_JOYPAD_A || id == RETRO_DEVICE_ID_JOYPAD_B);
    ++state_reads;
    return id == (unsigned)retro_pressed;
}

static bool frog_input(muos_button_t button) {
    ++frog_reads;
    return button == frog_pressed;
}

static void read_and_expect(lv_indev_t *indev, uint32_t key, lv_indev_state_t state) {
    lv_indev_data_t data;
    _lv_indev_read(indev, &data);
    assert(data.key == key && data.state == state);
}

static void check_retro_input(lv_indev_t *indev) {
    static const struct { int id; uint32_t key; } cases[] = {
        {RETRO_DEVICE_ID_JOYPAD_UP, LV_KEY_UP},
        {RETRO_DEVICE_ID_JOYPAD_DOWN, LV_KEY_DOWN},
        {RETRO_DEVICE_ID_JOYPAD_LEFT, LV_KEY_LEFT},
        {RETRO_DEVICE_ID_JOYPAD_RIGHT, LV_KEY_RIGHT},
        {RETRO_DEVICE_ID_JOYPAD_A, LV_KEY_ENTER},
        {RETRO_DEVICE_ID_JOYPAD_B, LV_KEY_ESC},
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        retro_pressed = cases[i].id;
        read_and_expect(indev, cases[i].key, LV_INDEV_STATE_PRESSED);
        assert(polls == i + 1 && state_reads == (i + 1) * MUOS_BTN_COUNT);
    }
    retro_pressed = -1;
    read_and_expect(indev, 0, LV_INDEV_STATE_RELEASED);
    assert(polls == 7 && state_reads == 7 * MUOS_BTN_COUNT);
}

static void check_frogui_input(lv_indev_t *indev) {
    static const struct { muos_button_t button; uint32_t key; } cases[] = {
        {MUOS_BTN_UP, LV_KEY_UP}, {MUOS_BTN_DOWN, LV_KEY_DOWN},
        {MUOS_BTN_LEFT, LV_KEY_LEFT}, {MUOS_BTN_RIGHT, LV_KEY_RIGHT},
        {MUOS_BTN_A, LV_KEY_ENTER}, {MUOS_BTN_B, LV_KEY_ESC},
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        frog_pressed = cases[i].button;
        read_and_expect(indev, cases[i].key, LV_INDEV_STATE_PRESSED);
        assert(polls == 0 && state_reads == 0);
        assert(frog_reads == (i + 1) * MUOS_BTN_COUNT);
    }
    frog_pressed = MUOS_BTN_COUNT;
    read_and_expect(indev, 0, LV_INDEV_STATE_RELEASED);
    assert(polls == 0 && state_reads == 0);
    assert(frog_reads == 7 * MUOS_BTN_COUNT);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    bool use_frogui = strcmp(argv[1], "frogui") == 0;
    assert(use_frogui || strcmp(argv[1], "libretro") == 0);

    lv_init();
    assert(!init_display_host());
    assert(!muos_retro_lvgl_attach(0, 4, video, poll, input_state, NULL));
    assert(!muos_retro_lvgl_attach(4, 0, video, poll, input_state, NULL));
    assert(!muos_retro_lvgl_attach(855, 4, video, poll, input_state, NULL));
    assert(!muos_retro_lvgl_attach(4, 481, video, poll, input_state, NULL));
    assert(!muos_retro_lvgl_attach(4, 4, NULL, poll, input_state, NULL));
    assert(muos_retro_lvgl_attach(4, 4, video, poll, input_state,
                                  use_frogui ? frog_input : NULL));
    assert(lv_disp_get_default()->driver->hor_res == 4);
    assert(lv_disp_get_default()->driver->ver_res == 4);
    assert(init_display_host());
    assert(device.mux.width == 4 && device.mux.height == 4);
    assert(svg_inits == 1);
    assert(mux_get_refresh_timer() == lv_disp_get_default()->refr_timer);

    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xff0000), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
    lv_refr_now(NULL);
    assert(video_frames > 0);
    for (unsigned i = 0; i < 16; ++i) assert(frame[i] == 0xf800);

    lv_color_t green[4] = {lv_color_hex(0x00ff00), lv_color_hex(0x00ff00),
                           lv_color_hex(0x00ff00), lv_color_hex(0x00ff00)};
    lv_area_t partial = {.x1 = 1, .y1 = 1, .x2 = 2, .y2 = 2};
    lv_disp_drv_t *display = lv_disp_get_default()->driver;
    display->flush_cb(display, &partial, green);
    assert(video_frames > 1);
    for (unsigned y = 0; y < 4; ++y)
        for (unsigned x = 0; x < 4; ++x) {
            assert(frame[y * 4 + x] == (x >= 1 && x <= 2 && y >= 1 && y <= 2
                                             ? 0x07e0 : 0xf800));
        }

    lv_indev_t *indev = lv_indev_get_next(NULL);
    assert(indev);
    if (use_frogui) check_frogui_input(indev);
    else check_retro_input(indev);
    return 0;
}
