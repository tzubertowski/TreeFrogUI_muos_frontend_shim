#ifndef MUOS_RETRO_LVGL_BRIDGE_H
#define MUOS_RETRO_LVGL_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>
#include "libretro.h"

typedef enum {
    MUOS_BTN_UP, MUOS_BTN_DOWN, MUOS_BTN_LEFT, MUOS_BTN_RIGHT,
    MUOS_BTN_A, MUOS_BTN_B, MUOS_BTN_COUNT
} muos_button_t;
typedef bool (*muos_pressed_cb)(muos_button_t button);

/* Attach LVGL 8 to libretro callbacks. Call after lv_init(), before creating UI. */
bool muos_retro_lvgl_attach(unsigned width, unsigned height,
                            retro_video_refresh_t video,
                            retro_input_poll_t poll,
                            retro_input_state_t input,
                            muos_pressed_cb pressed);

#endif
