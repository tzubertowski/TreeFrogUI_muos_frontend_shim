#include <assert.h>
#include "bridge_logic.h"

int main(void) {
    bool buttons[MUOS_BTN_COUNT] = {false};
    assert(muos_rgb888_to_rgb565(255, 0, 0) == 0xf800);
    assert(muos_rgb888_to_rgb565(0, 255, 0) == 0x07e0);
    assert(muos_rgb888_to_rgb565(0, 0, 255) == 0x001f);
    assert(muos_rgb888_to_rgb565(255, 255, 255) == 0xffff);
    assert(muos_key_from_buttons(buttons) == 0);
    buttons[MUOS_BTN_DOWN] = true;
    assert(muos_key_from_buttons(buttons) == LV_KEY_DOWN);
    buttons[MUOS_BTN_A] = true;
    assert(muos_key_from_buttons(buttons) == LV_KEY_DOWN);
    return 0;
}
