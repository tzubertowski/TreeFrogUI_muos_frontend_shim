#include <assert.h>
#include <stdint.h>
#include "common/config/config.h"
#include "common/platform/board.h"
#include "common/platform/device.h"
#include "common/platform/input.h"
#include "common/runtime/init.h"

struct mux_config config;
struct mux_device device;
int msgbox_active, g350_menu_pressed;
static uint64_t now_ms;
static struct { mux_input_type type; mux_input_action action; } events[16];
static unsigned event_count, combo_events[8], combo_count;

uint64_t mux_tick(void) { return now_ms; }
int board_layout_map_swap(void) { return 0; }
board_special_t board_special(void) { return board_special_none; }
int board_is(board_special_t type) { return type == board_special_none; }
int video_preview_active(void) { return 0; }
void video_preview_cancel(void) {}
int SDL_PushEvent(SDL_Event *event) { (void)event; return 0; }

static void on_input(mux_input_type type, mux_input_action action) {
    assert(event_count < sizeof(events) / sizeof(events[0]));
    events[event_count++] = (typeof(events[0])){type, action};
}

static void on_combo(int index, mux_input_action action) {
    assert(index == 0 && combo_count < sizeof(combo_events) / sizeof(combo_events[0]));
    combo_events[combo_count++] = action;
}

static int begin_temporary_options(void) {
    mux_input_options opts = {0};
    opts.input_handler = on_input;
    opts.hold_disabled = 1;
    return mux_input_host_begin(&opts);
}

int main(void) {
    mux_input_options opts = {0};
    opts.input_handler = on_input;
    opts.combo_handler = on_combo;
    opts.combo_count = 1;
    opts.combo[0].type_mask = BIT(mux_input_a) | BIT(mux_input_b);
    opts.hold_disabled = 1;
    config.settings.advanced.repeat_delay = 80;

    assert(!mux_input_host_begin(NULL));
    mux_input_host_step(BIT(mux_input_dpad_up));
    assert(mux_input_host_begin(&opts));
    assert(!mux_input_host_begin(&opts));
    assert(opts.nav == NAV_DPAD);

    now_ms = 100;
    mux_input_host_step(BIT(mux_input_dpad_up) | (UINT64_C(1) << mux_input_count));
    assert(event_count == 1 && events[0].type == mux_input_dpad_up &&
           events[0].action == mux_input_press);
    now_ms = 179;
    mux_input_host_step(BIT(mux_input_dpad_up));
    assert(event_count == 1);
    now_ms = 180;
    mux_input_host_step(BIT(mux_input_dpad_up));
    assert(event_count == 2 && events[1].action == mux_input_hold);
    now_ms = 181;
    mux_input_host_step(0);
    assert(event_count == 3 && events[2].action == mux_input_release);

    now_ms = 300;
    mux_input_host_step(BIT(mux_input_a) | BIT(mux_input_b));
    assert(combo_count == 1 && combo_events[0] == mux_input_press);
    now_ms = 396;
    mux_input_host_step(BIT(mux_input_a) | BIT(mux_input_b));
    assert(combo_count == 2 && combo_events[1] == mux_input_hold);
    now_ms = 397;
    mux_input_host_step(BIT(mux_input_a));
    assert(combo_count == 3 && combo_events[2] == mux_input_release);
    mux_input_host_end();
    mux_input_host_end();
    assert(!mux_input_host_stopped());

    event_count = 0;
    assert(begin_temporary_options());
    now_ms = 500;
    mux_input_host_step(BIT(mux_input_a));
    assert(event_count == 1 && events[0].type == mux_input_a);
    mux_input_stop();
    assert(mux_input_host_stopped());
    mux_input_host_step(0);
    assert(event_count == 1);
    mux_input_host_end();
    assert(!mux_input_host_stopped());

    assert(begin_temporary_options());
    assert(!mux_input_host_stopped());
    now_ms = 600;
    mux_input_host_step(BIT(mux_input_b));
    assert(event_count == 2 && events[1].type == mux_input_b);
    mux_input_host_end();
    return 0;
}
