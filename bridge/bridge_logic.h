#ifndef MUOS_BRIDGE_LOGIC_H
#define MUOS_BRIDGE_LOGIC_H

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"
#include "retro_lvgl_bridge.h"

uint16_t muos_rgb888_to_rgb565(uint8_t red, uint8_t green, uint8_t blue);
uint32_t muos_key_from_buttons(const bool pressed[MUOS_BTN_COUNT]);

#endif
