#include <assert.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "libretro.h"
#include "common/base/options.h"

typedef void (*void_fn)(void);
typedef bool (*load_game_fn)(const struct retro_game_info *);
typedef void (*set_environment_fn)(retro_environment_t);
typedef void (*set_video_fn)(retro_video_refresh_t);
typedef void (*set_poll_fn)(retro_input_poll_t);
typedef void (*set_state_fn)(retro_input_state_t);
typedef void (*route_fn)(const char *);
typedef int (*screen_fn)(void);

static uint64_t buttons;
static unsigned frames;
static bool shutdown_requested;

static bool environment(unsigned command, void *data) {
    (void)data;
    if (command == RETRO_ENVIRONMENT_SHUTDOWN) shutdown_requested = true;
    return true;
}

static void video(const void *data, unsigned width, unsigned height, size_t pitch) {
    assert(data && width == 640 && height == 480 && pitch == width * 2);
    ++frames;
}

static void poll(void) {}

static int16_t input(unsigned port, unsigned device, unsigned index, unsigned id) {
    (void)port;
    (void)device;
    (void)index;
    return id < 64 && (buttons & (UINT64_C(1) << id)) != 0;
}

static void tap(void_fn run, const unsigned button) {
    buttons = UINT64_C(1) << button;
    run();
    buttons = 0;
    for (int i = 0; i < 24; ++i) run();
}

static void fixture(void) {
    char path[512];
    mkdir(OPT_PATH, 0755);
    mkdir(OPT_PATH "device", 0755);
    mkdir(OPT_PATH "device/config", 0755);
    mkdir(OPT_PATH "device/config/mux", 0755);
    mkdir(OPT_PATH "config", 0755);
    mkdir(OPT_PATH "config/visual", 0755);
    mkdir(OPT_PATH "config/settings", 0755);
    mkdir(OPT_PATH "config/settings/general", 0755);
    mkdir(OPT_PATH "config/settings/theme", 0755);
    mkdir(OPT_PATH "config/theme", 0755);
    mkdir(OPT_PATH "share", 0755);
    mkdir(OPT_PATH "share/theme", 0755);
    mkdir(INTERNAL_THEME, 0755);
    mkdir(OPT_PATH "share/font", 0755);
    mkdir(INTERNAL_FONTS, 0755);

    snprintf(path, sizeof(path), "%swidth", CONF_DEVICE_PATH "mux/");
    FILE *file = fopen(path, "w"); assert(file); fputs("640\n", file); fclose(file);
    snprintf(path, sizeof(path), "%sheight", CONF_DEVICE_PATH "mux/");
    file = fopen(path, "w"); assert(file); fputs("480\n", file); fclose(file);
    snprintf(path, sizeof(path), "%sblackfade", CONF_CONFIG_PATH "visual/");
    file = fopen(path, "w"); assert(file); fputs("0\n", file); fclose(file);
    snprintf(path, sizeof(path), "%sorientation", CONF_CONFIG_PATH "settings/general/");
    file = fopen(path, "w"); assert(file); fputs("0\n", file); fclose(file);
    snprintf(path, sizeof(path), "%sactive", CONF_CONFIG_PATH "theme/");
    file = fopen(path, "w"); assert(file); fputs("MinUIfied.-.Horizontal\n", file); fclose(file);
    snprintf(path, sizeof(path), "%smenu_style", CONF_CONFIG_PATH "settings/theme/");
    file = fopen(path, "w"); assert(file); fputs("0\n", file); fclose(file);
    snprintf(path, sizeof(path), "%sversion.txt", INTERNAL_THEME);
    file = fopen(path, "w"); assert(file); fputs("Edge\n", file); fclose(file);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    fixture();

    void *core = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!core) { fprintf(stderr, "%s\n", dlerror()); return 2; }
#define SYM(type, name) ((type)dlsym(core, name))
    set_environment_fn set_environment = SYM(set_environment_fn, "retro_set_environment");
    set_video_fn set_video = SYM(set_video_fn, "retro_set_video_refresh");
    set_poll_fn set_poll = SYM(set_poll_fn, "retro_set_input_poll");
    set_state_fn set_state = SYM(set_state_fn, "retro_set_input_state");
    void_fn init = SYM(void_fn, "retro_init");
    void_fn run = SYM(void_fn, "retro_run");
    void_fn deinit = SYM(void_fn, "retro_deinit");
    load_game_fn load = SYM(load_game_fn, "retro_load_game");
    route_fn route = SYM(route_fn, "retro_test_route");
    screen_fn current_screen = SYM(screen_fn, "retro_test_current_screen");
    assert(set_environment && set_video && set_poll && set_state && init && run && deinit && load && route &&
           current_screen);

    set_environment(environment);
    set_video(video);
    set_poll(poll);
    set_state(input);
    init();
    assert(load(NULL));
    run();

    route("collection");
    run();
    assert(current_screen() == 2);

    tap(run, RETRO_DEVICE_ID_JOYPAD_Y);      /* Open MuOS's create-collection keyboard. */
    tap(run, RETRO_DEVICE_ID_JOYPAD_DOWN);   /* Select the first letter, q. */
    tap(run, RETRO_DEVICE_ID_JOYPAD_A);
    tap(run, RETRO_DEVICE_ID_JOYPAD_A);      /* Type qq without bypassing the OSK. */
    tap(run, RETRO_DEVICE_ID_JOYPAD_START);  /* MuOS confirms the collection name. */
    for (int i = 0; i < 24; ++i) run();

    char collection[512];
    snprintf(collection, sizeof(collection), "%s/qq", INFO_COL_PATH);
    struct stat info;
    assert(stat(collection, &info) == 0 && S_ISDIR(info.st_mode));
    assert(frames > 10);

    deinit();
    dlclose(core);
    assert(!shutdown_requested);
    puts("QEMU collection OSK flow passed (collection directory persisted)");
    return 0;
}
