#include "retro_lvgl_bridge.h"

#include <string.h>
#include "lvgl.h"
#include "bridge_logic.h"
#include "muos_host_diag.h"

#define MAX_WIDTH  854
#define MAX_HEIGHT 480

static uint16_t framebuffer[MAX_WIDTH * MAX_HEIGHT];
static lv_color_t draw_buffer[MAX_WIDTH * MAX_HEIGHT];
static lv_disp_draw_buf_t display_buffer;
static lv_disp_drv_t display_driver;
static lv_indev_drv_t input_driver;
static retro_video_refresh_t video_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
static muos_pressed_cb pressed_cb;
static unsigned screen_width, screen_height;

static void flush(lv_disp_drv_t *driver, const lv_area_t *area,
                  lv_color_t *pixels) {
    static unsigned flushes;
    (void)driver;
    for (int y = area->y1; y <= area->y2; ++y) {
        for (int x = area->x1; x <= area->x2; ++x) {
            lv_color32_t rgba = {.full = lv_color_to32(*pixels++)};
            framebuffer[y * screen_width + x] =
                muos_rgb888_to_rgb565(rgba.ch.red, rgba.ch.green, rgba.ch.blue);
        }
    }
    ++flushes;
    if (flushes <= 6)
        muos_diag_log("video_flush", "n=%u area=%d,%d-%d,%d panel=%ux%u corner=%04x center=%04x",
                      flushes, area->x1, area->y1, area->x2, area->y2, screen_width, screen_height,
                      framebuffer[0], framebuffer[(screen_height / 2) * screen_width + screen_width / 2]);
    if (video_cb)
        video_cb(framebuffer, screen_width, screen_height,
                 screen_width * sizeof(framebuffer[0]));
    lv_disp_flush_ready(driver);
}

static bool pressed(muos_button_t button) {
    static const unsigned retro_ids[MUOS_BTN_COUNT] = {
        RETRO_DEVICE_ID_JOYPAD_UP, RETRO_DEVICE_ID_JOYPAD_DOWN,
        RETRO_DEVICE_ID_JOYPAD_LEFT, RETRO_DEVICE_ID_JOYPAD_RIGHT,
        RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_B
    };
    if (pressed_cb) return pressed_cb(button);
    return input_state_cb && button < MUOS_BTN_COUNT &&
           input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, retro_ids[button]);
}

static void read_input(lv_indev_drv_t *driver, lv_indev_data_t *data) {
    (void)driver;
    if (!pressed_cb && input_poll_cb) input_poll_cb();
    bool buttons[MUOS_BTN_COUNT];
    for (unsigned i = 0; i < MUOS_BTN_COUNT; ++i)
        buttons[i] = pressed((muos_button_t)i);
    data->key = muos_key_from_buttons(buttons);
    data->state = data->key ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

bool muos_retro_lvgl_attach(unsigned width, unsigned height,
                            retro_video_refresh_t video,
                            retro_input_poll_t poll,
                            retro_input_state_t input,
                            muos_pressed_cb pressed) {
    if (!video || width == 0 || height == 0 || width > MAX_WIDTH ||
        height > MAX_HEIGHT)
        return false;

    screen_width = width;
    screen_height = height;
    video_cb = video;
    input_poll_cb = poll;
    input_state_cb = input;
    pressed_cb = pressed;
    memset(framebuffer, 0, sizeof(framebuffer));

    lv_disp_draw_buf_init(&display_buffer, draw_buffer, NULL,
                          screen_width * screen_height);
    lv_disp_drv_init(&display_driver);
    display_driver.hor_res = screen_width;
    display_driver.ver_res = screen_height;
    display_driver.draw_buf = &display_buffer;
    display_driver.flush_cb = flush;
    lv_disp_drv_register(&display_driver);

    lv_indev_drv_init(&input_driver);
    input_driver.type = LV_INDEV_TYPE_KEYPAD;
    input_driver.read_cb = read_input;
    lv_indev_drv_register(&input_driver);
    return true;
}
