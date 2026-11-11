#include "bridge_logic.h"

uint16_t muos_rgb888_to_rgb565(uint8_t red, uint8_t green, uint8_t blue) {
    return (uint16_t)(((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3));
}

uint32_t muos_key_from_buttons(const bool pressed[MUOS_BTN_COUNT]) {
    static const uint32_t keys[MUOS_BTN_COUNT] = {
        LV_KEY_UP, LV_KEY_DOWN, LV_KEY_LEFT, LV_KEY_RIGHT, LV_KEY_ENTER, LV_KEY_ESC
    };
    for (unsigned i = 0; i < MUOS_BTN_COUNT; ++i)
        if (pressed[i]) return keys[i];
    return 0;
}
