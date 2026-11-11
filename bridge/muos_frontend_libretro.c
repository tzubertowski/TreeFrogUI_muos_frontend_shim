#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "libretro.h"
#include "lvgl.h"
#include "common/base/options.h"
#include "common/platform/input.h"
#include "common/platform/device.h"
#include "common/runtime/init.h"
#include "common/ui/common.h"
#include "common/display/image.h"
#include "common/display/theme.h"
#include "common/config/config.h"
#include "common/content/core/common.h"
#include "common/ui/nav/menu_style.h"
#include "common/ui/image.h"
#include "common/storage/fileio.h"
#include "muos_host_diag.h"
#include "module/muxshare.h"
#include "retro_lvgl_bridge.h"
#include "muos_host_supervisor.h"

static retro_video_refresh_t video_cb;
static retro_environment_t environment_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
static bool started;
typedef enum {
    host_screen_launcher,
    host_screen_explore,
    host_screen_collection,
    host_screen_history,
    host_screen_app,
    host_screen_info,
    host_screen_config,
    host_screen_search,
    host_screen_assign,
    host_screen_tag,
    host_screen_option,
    host_screen_order,
} host_screen;
static host_screen current_screen;
static host_screen parent_screen = host_screen_launcher;
#define RETRO_API __attribute__((visibility("default")))

static unsigned panel_dimension(const char *name, unsigned fallback, unsigned limit) {
    const char *value = getenv(name);
    if (!value || !*value) return fallback;

    char *end = NULL;
    const unsigned long parsed = strtoul(value, &end, 10);
    if (end == value || *end || parsed == 0 || parsed > limit) return fallback;
    return (unsigned)parsed;
}

static unsigned panel_width(void) { return panel_dimension("TF_PANEL_W", 320, 854); }
static unsigned panel_height(void) { return panel_dimension("TF_PANEL_H", 240, 480); }

static bool runtime_ready(void) {
    return access(CONF_DEVICE_PATH "mux", R_OK) == 0 &&
           access(INTERNAL_THEME "/version.txt", R_OK) == 0 &&
           access(INTERNAL_FONTS, R_OK) == 0;
}

static uint64_t read_buttons(void) {
    static const unsigned ids[mux_input_count] = {
        [mux_input_a] = RETRO_DEVICE_ID_JOYPAD_A,
        [mux_input_b] = RETRO_DEVICE_ID_JOYPAD_B,
        [mux_input_x] = RETRO_DEVICE_ID_JOYPAD_X,
        [mux_input_y] = RETRO_DEVICE_ID_JOYPAD_Y,
        [mux_input_l1] = RETRO_DEVICE_ID_JOYPAD_L,
        [mux_input_r1] = RETRO_DEVICE_ID_JOYPAD_R,
        [mux_input_l2] = RETRO_DEVICE_ID_JOYPAD_L2,
        [mux_input_r2] = RETRO_DEVICE_ID_JOYPAD_R2,
        [mux_input_select] = RETRO_DEVICE_ID_JOYPAD_SELECT,
        [mux_input_start] = RETRO_DEVICE_ID_JOYPAD_START,
        [mux_input_dpad_up] = RETRO_DEVICE_ID_JOYPAD_UP,
        [mux_input_dpad_down] = RETRO_DEVICE_ID_JOYPAD_DOWN,
        [mux_input_dpad_left] = RETRO_DEVICE_ID_JOYPAD_LEFT,
        [mux_input_dpad_right] = RETRO_DEVICE_ID_JOYPAD_RIGHT,
    };
    static const unsigned mapped[] = {
        mux_input_a, mux_input_b, mux_input_x, mux_input_y,
        mux_input_l1, mux_input_r1, mux_input_l2, mux_input_r2,
        mux_input_select, mux_input_start, mux_input_dpad_up,
        mux_input_dpad_down, mux_input_dpad_left, mux_input_dpad_right
    };
    uint64_t mask = 0;
    if (input_poll_cb) input_poll_cb();
    for (unsigned i = 0; i < sizeof(mapped) / sizeof(mapped[0]); ++i)
        if (input_state_cb && input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, ids[mapped[i]]))
            mask |= BIT(mapped[i]);
    return mask;
}

static int host_take_action(char *action, size_t size) {
    FILE *file = fopen(MUOS_ACT_LOAD, "r");
    if (!file) return 0;
    const int read = fgets(action, (int)size, file) != NULL;
    fclose(file);
    if (!read) return 0;
    action[strcspn(action, "\r\n")] = '\0';
    return action[0] != '\0';
}

static const char *host_screen_name(host_screen screen) {
    static const char *const names[] = {"muxlaunch", "muxplore", "muxcollect", "muxhistory", "muxapp", "muxinfo",
                                        "muxconfig", "muxsearch", "muxassign", "muxtag", "muxoption", "muxorder"};
    return screen >= host_screen_launcher && screen <= host_screen_order ? names[screen] : "unknown";
}

static int host_screen_is_nested(host_screen screen) {
    return screen >= host_screen_search && screen <= host_screen_order;
}

static void host_screen_cleanup(void) {
    if (toast_timer) { lv_timer_del(toast_timer); toast_timer = NULL; }
    if (counter_timer) { lv_timer_del(counter_timer); counter_timer = NULL; }
    if (key_entry && lv_obj_is_valid(key_entry)) { lv_obj_del(key_entry); key_entry = NULL; }
    if (num_entry && lv_obj_is_valid(num_entry)) { lv_obj_del(num_entry); num_entry = NULL; }
    timer_suspend_all();
    ui_screen_teardown();
    if (ui_screen_temp) lv_disp_load_scr(ui_screen_temp);
    if (ui_screen_container && lv_obj_is_valid(ui_screen_container)) lv_obj_del(ui_screen_container);
    ui_screen_container = NULL;
    msgbox_active = 0;
    ui_count_static = 0;
    grid_mode_enabled = 0;
    current_item_index = 0;
    first_open = 1;
}

static void host_screen_end(void) {
    switch (current_screen) {
        case host_screen_explore: muxplore_host_end(); break;
        case host_screen_collection: muxcollect_host_end(); break;
        case host_screen_history: muxhistory_host_end(); break;
        case host_screen_app: muxapp_host_end(); break;
        case host_screen_info: muxinfo_host_end(); break;
        case host_screen_config: muxconfig_host_end(); break;
        case host_screen_search: muxsearch_host_end(); break;
        case host_screen_assign: muxassign_host_end(); break;
        case host_screen_tag: muxtag_host_end(); break;
        case host_screen_option: muxoption_host_end(); break;
        case host_screen_order: muxorder_host_end(); break;
        case host_screen_launcher: muxlaunch_host_end(); break;
    }
    host_screen_cleanup();
}

static int host_screen_start(const host_screen screen) {
    int ok = 0;
    char *name = NULL, *dir = NULL, *sys = NULL;
    switch (screen) {
        case host_screen_launcher: ok = muxlaunch_host_start(); break;
        case host_screen_explore: ok = muxplore_host_start(0, NULL); break;
        case host_screen_collection: ok = muxcollect_host_start(0, "", -1); break;
        case host_screen_history: ok = muxhistory_host_start(-1); break;
        case host_screen_app: ok = muxapp_host_start(); break;
        case host_screen_info: ok = muxinfo_host_start(); break;
        case host_screen_config: ok = muxconfig_host_start(); break;
        case host_screen_search:
            dir = read_line_char_from(EXPLORE_DIR, 1);
            ok = muxsearch_host_start(dir && *dir ? dir : sys_dir);
            free(dir);
            break;
        case host_screen_assign:
            name = read_line_char_from(MUOS_ASS_LOAD, 1);
            dir = read_line_char_from(MUOS_ASS_LOAD, 2);
            sys = read_line_char_from(MUOS_ASS_LOAD, 3);
            ok = muxassign_host_start(0, name ? name : "", dir ? dir : sys_dir, sys ? sys : "",
                                      read_line_int_from(MUOS_ASS_LOAD, 5));
            free(name); free(dir); free(sys);
            break;
        case host_screen_tag:
            name = read_line_char_from(MUOS_HST_LOAD, 1);
            ok = muxtag_host_start(0, name ? name : "", sys_dir, "", 0);
            free(name);
            break;
        case host_screen_option:
            name = read_line_char_from(MUOS_HST_LOAD, 1);
            ok = muxoption_host_start(0, name ? name : "", sys_dir, "", 0);
            free(name);
            break;
        case host_screen_order: ok = muxorder_host_start(); break;
    }
    if (!ok) return 0;
    current_screen = screen;
    muos_diag_log("screen", "started module=%s", host_screen_name(screen));
    return 1;
}

static int host_screen_from_action(const char *action, host_screen *screen) {
    static const struct { const char *action; host_screen screen; } routes[] = {
        {"launcher", host_screen_launcher}, {"explore", host_screen_explore},
        {"collection", host_screen_collection}, {"history", host_screen_history},
        {"app", host_screen_app}, {"info", host_screen_info}, {"config", host_screen_config},
        {"search", host_screen_search}, {"assign", host_screen_assign}, {"tag", host_screen_tag},
        {"option", host_screen_option}, {"order", host_screen_order},
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); ++i) {
        if (strcmp(action, routes[i].action) == 0) {
            *screen = routes[i].screen;
            return 1;
        }
    }
    return 0;
}

static void host_exit_chord(void) {
    FILE *file = fopen(MUOS_ACT_LOAD, "w");
    if (file) {
        fputs("launcher\n", file);
        fclose(file);
        mux_input_stop();
    }
}

static void host_dispatch_action(void) {
    char action[64] = {0};
    host_screen next_screen = current_screen;
    if (!host_take_action(action, sizeof(action))) {
        next_screen = host_screen_is_nested(current_screen) ? parent_screen : host_screen_launcher;
        muos_diag_log("screen", "stopped_without_action from=%s", host_screen_name(current_screen));
    } else {
        muos_diag_log("screen", "dispatch action=%s from=%s", action, host_screen_name(current_screen));
        if (strcmp(action, "shutdown") == 0 || strcmp(action, "reboot") == 0) return;
        const int routed = host_screen_from_action(action, &next_screen);
        if (routed && host_screen_is_nested(next_screen)) parent_screen = current_screen;
        if (!routed) {
            muos_diag_log("screen", "unsupported action=%s from=%s", action, host_screen_name(current_screen));
            next_screen = current_screen;
        }

        if (routed && (next_screen == host_screen_explore || next_screen == host_screen_history ||
                       next_screen == host_screen_collection) && access(MUOS_ROM_LOAD, R_OK) == 0) {
            host_screen_end();
            started = false;
            if (muos_host_launch_game(environment_cb)) return;
            unlink(MUOS_ROM_LOAD);
            unlink(MUOS_ACT_LOAD);
            muos_diag_log("launch", "game handoff unavailable; returning to %s", host_screen_name(next_screen));
            if (host_screen_start(next_screen)) started = true;
            return;
        }
        unlink(MUOS_ACT_LOAD);
    }

    host_screen_end();
    if (!host_screen_start(next_screen)) {
        muos_diag_log("screen", "start_failed module=%s", next_screen == host_screen_explore ? "muxplore"
                                                                                               : host_screen_name(next_screen));
        started = false;
    }
}

#ifdef MUOS_HOST_TEST
RETRO_API void retro_test_route(const char *action) {
    FILE *file = fopen(MUOS_ACT_LOAD, "w");
    if (!file) return;
    fprintf(file, "%s\n", action);
    fclose(file);
    mux_input_stop();
}

RETRO_API int retro_test_auto_assign(char *rom_dir) { return automatic_assign_core(rom_dir); }
RETRO_API int retro_test_launch_game(void) { return muos_host_launch_game(environment_cb); }
RETRO_API int retro_test_selected_index(void) { return current_item_index; }
RETRO_API int retro_test_current_screen(void) { return current_screen; }
#endif

RETRO_API unsigned retro_api_version(void) { return RETRO_API_VERSION; }
RETRO_API void retro_get_system_info(struct retro_system_info *i) {
    *i = (struct retro_system_info){"muOS Frontend MVP", "0.1", "", false, false};
}
RETRO_API void retro_get_system_av_info(struct retro_system_av_info *i) {
    const unsigned width = panel_width();
    const unsigned height = panel_height();
    *i = (struct retro_system_av_info){
        .geometry = {width, height, width, height, (float)width / height},
        .timing = {60.0, 48000.0}
    };
}
RETRO_API void retro_set_environment(retro_environment_t cb) {
    environment_cb = cb;
    if (cb) {
        bool no_game = true;
        enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
        cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_game);
        cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
    }
}
RETRO_API void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
RETRO_API void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { (void)cb; }
RETRO_API void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
RETRO_API void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }
RETRO_API void retro_init(void) {
    muos_diag_start();
    const bool ready = runtime_ready();
    muos_diag_log("retro_init", "enter panel=%ux%u runtime_ready=%d video_cb=%d opt=%s run=%s",
                  panel_width(), panel_height(), ready, video_cb != NULL, OPT_PATH, RUN_PATH);
    muos_diag_log("build", "TEST_IMAGE=%d", TEST_IMAGE);
    if (!ready || !video_cb) {
        muos_diag_log("retro_init", "aborted before LVGL startup");
        return;
    }
    unlink(MUOS_ACT_LOAD);
    unlink(MUOS_ROM_LOAD);
    lv_init();
    img_init();
    muos_diag_log("retro_init", "img_init complete");
    const bool attached = muos_retro_lvgl_attach(panel_width(), panel_height(), video_cb, NULL, NULL, NULL);
    muos_diag_log("retro_init", "lvgl_attach=%d", attached);
    if (!attached) return;
    const bool runtime = init_runtime_host();
    muos_diag_log("retro_init", "init_runtime_host=%d", runtime);
    if (!runtime) return;
    snprintf(device.storage.rom.mount, sizeof(device.storage.rom.mount), "/mnt/sdcard");
    snprintf(device.storage.sdcard.mount, sizeof(device.storage.sdcard.mount), "/mnt/sdcard");
    muos_diag_log("storage", "host mounts rom=%s sdcard=%s", device.storage.rom.mount, device.storage.sdcard.mount);
    const bool menu = host_screen_start(host_screen_launcher);
    muos_diag_log("retro_init", "muxlaunch_host_start=%d", menu);
    muos_diag_log("theme_layout", "module=%s menu_style=%d active=%d grid=%d:%dx%d nav=%d/%d cell_text=%u/%u label_text=%u list_text=%u/%u",
                  mux_module,
                  config.settings.themeopt.menu_style, menu_style_active(), theme.grid.enabled, theme.grid.row_count,
                  theme.grid.column_count, theme.misc.navigation_type, theme.grid.navigation_type,
                  theme.grid.cell_default.text_alpha, theme.grid.cell_focus.text_alpha,
                  theme.grid.current_item_label.text_alpha,
                  theme.list_default.text_alpha, theme.list_focus.text_alpha);
    if (!menu) return;
    started = true;
    muos_diag_log("retro_init", "frontend started");
}
RETRO_API void retro_deinit(void) {
    muos_diag_log("retro_deinit", "started=%d", started);
    if (started) host_screen_end();
    image_async_shutdown();
    started = false;
}
RETRO_API bool retro_load_game(const struct retro_game_info *info) { (void)info; return true; }
RETRO_API void retro_unload_game(void) {}
RETRO_API void retro_run(void) {
    if (!started) return;
    static unsigned frames;
    ++frames;
    if (frames <= 3 || frames % 300 == 0) muos_diag_log("retro_run", "frame=%u", frames);
    lv_tick_inc(16);
    static uint64_t previous_buttons;
    uint64_t buttons = read_buttons();
    if (buttons != previous_buttons) {
        muos_diag_log("input", "screen=%s mask=0x%llx", host_screen_name(current_screen),
                      (unsigned long long) buttons);
        previous_buttons = buttons;
    }
    const uint64_t exit_chord = BIT(mux_input_select) | BIT(mux_input_start);
    if ((buttons & exit_chord) == exit_chord) {
        buttons &= ~exit_chord;
        if (current_screen != host_screen_launcher && !mux_input_host_stopped()) host_exit_chord();
    }
    mux_input_host_step(buttons);
    if (mux_input_host_stopped()) host_dispatch_action();
    lv_timer_handler();
    muos_host_supervisor_step(environment_cb);
}
RETRO_API void retro_reset(void) {}
RETRO_API unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
RETRO_API size_t retro_serialize_size(void) { return 0; }
RETRO_API bool retro_serialize(void *d, size_t s) { (void)d; (void)s; return false; }
RETRO_API bool retro_unserialize(const void *d, size_t s) { (void)d; (void)s; return false; }
RETRO_API void retro_cheat_reset(void) {}
RETRO_API void retro_cheat_set(unsigned i, bool e, const char *c) { (void)i; (void)e; (void)c; }
RETRO_API void *retro_get_memory_data(unsigned id) { (void)id; return NULL; }
RETRO_API size_t retro_get_memory_size(unsigned id) { (void)id; return 0; }
RETRO_API bool retro_load_game_special(unsigned t, const struct retro_game_info *i, size_t n) {
    (void)t; (void)i; (void)n; return false;
}
