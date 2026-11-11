#include <assert.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "libretro.h"
#include "common/base/options.h"
#include "muos_host_supervisor.h"

typedef void (*set_environment_fn)(retro_environment_t);
typedef void (*set_video_fn)(retro_video_refresh_t);
typedef void (*set_poll_fn)(retro_input_poll_t);
typedef void (*set_state_fn)(retro_input_state_t);
typedef void (*void_fn)(void);
typedef bool (*load_game_fn)(const struct retro_game_info *);
typedef void (*get_av_info_fn)(struct retro_system_av_info *);

static unsigned frames, polls;
static unsigned nonblack;
static unsigned expected_width = 320, expected_height = 240;
static uint64_t first_hash, current_hash;
static uint64_t pressed_buttons;
static bool shutdown_requested;
static bool saw_changed_hash;
static bool environment(unsigned command, void *data) {
    (void)data;
    if (command == RETRO_ENVIRONMENT_SHUTDOWN) shutdown_requested = true;
    return true;
}
static void video(const void *data, unsigned width, unsigned height, size_t pitch) {
    assert(width == expected_width && height == expected_height && pitch == width * 2);
    const uint16_t *p = data;
    nonblack = 0;
    current_hash = UINT64_C(1469598103934665603);
    for (unsigned i = 0; i < width * height; ++i) {
        nonblack += p[i] != 0;
        current_hash = (current_hash ^ p[i]) * UINT64_C(1099511628211);
    }
    if (!frames) first_hash = current_hash;
    else if (current_hash != first_hash) saw_changed_hash = true;
    ++frames;
}
static void poll(void) { ++polls; }
static int16_t input(unsigned port, unsigned device, unsigned index, unsigned id) {
    (void)port; (void)device; (void)index;
    return (pressed_buttons & (UINT64_C(1) << id)) != 0;
}
static void tap(void_fn run, unsigned id) {
    pressed_buttons = UINT64_C(1) << id;
    run();
    pressed_buttons = 0;
    for (unsigned i = 0; i < 8; ++i) run();
}
static void fixture(void) {
    char p[512];
    mkdir(OPT_PATH, 0755);
    mkdir(OPT_PATH "device", 0755); mkdir(OPT_PATH "device/config", 0755);
    mkdir(OPT_PATH "device/config/mux", 0755); mkdir(OPT_PATH "config", 0755);
    mkdir(OPT_PATH "config/visual", 0755); mkdir(OPT_PATH "config/settings", 0755);
    mkdir(OPT_PATH "config/settings/general", 0755); mkdir(OPT_PATH "share", 0755);
    mkdir(OPT_PATH "config/settings/theme", 0755);
    mkdir(OPT_PATH "share/theme", 0755); mkdir(INTERNAL_THEME, 0755);
    mkdir(OPT_PATH "share/font", 0755); mkdir(INTERNAL_FONTS, 0755);
    snprintf(p, sizeof p, "%swidth", CONF_DEVICE_PATH "mux/");
    FILE *f = fopen(p, "w"); assert(f); fprintf(f, "%u\n", expected_width); fclose(f);
    snprintf(p, sizeof p, "%sheight", CONF_DEVICE_PATH "mux/");
    f = fopen(p, "w"); assert(f); fprintf(f, "%u\n", expected_height); fclose(f);
    snprintf(p, sizeof p, "%sblackfade", CONF_CONFIG_PATH "visual/");
    f = fopen(p, "w"); assert(f); fputs("0\n", f); fclose(f);
    snprintf(p, sizeof p, "%sorientation", CONF_CONFIG_PATH "settings/general/");
    f = fopen(p, "w"); assert(f); fputs("0\n", f); fclose(f);
    mkdir(CONF_CONFIG_PATH "theme", 0755);
    snprintf(p, sizeof p, "%sactive", CONF_CONFIG_PATH "theme/");
    f = fopen(p, "w"); assert(f); fputs("MinUIfied.-.Horizontal\n", f); fclose(f);
    snprintf(p, sizeof p, "%smenu_style", CONF_CONFIG_PATH "settings/theme/");
    f = fopen(p, "w"); assert(f); fputs("0\n", f); fclose(f);
    snprintf(p, sizeof p, "%s/version.txt", INTERNAL_THEME);
    f = fopen(p, "w"); assert(f); fputs("Edge\n", f); fclose(f);
}
#define SYM(handle, type, name) ((type)dlsym((handle), name))
int main(int argc, char **argv) {
    assert(argc == 2);
    const char *w = getenv("TF_PANEL_W"), *height_env = getenv("TF_PANEL_H");
    if (w) expected_width = (unsigned)strtoul(w, NULL, 10);
    if (height_env) expected_height = (unsigned)strtoul(height_env, NULL, 10);
    fixture();
    void *h = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!h) { fprintf(stderr, "%s\n", dlerror()); return 2; }
    set_environment_fn set_env = SYM(h, set_environment_fn, "retro_set_environment");
    set_video_fn set_video = SYM(h, set_video_fn, "retro_set_video_refresh");
    set_poll_fn set_poll = SYM(h, set_poll_fn, "retro_set_input_poll");
    set_state_fn set_state = SYM(h, set_state_fn, "retro_set_input_state");
    get_av_info_fn get_av_info = SYM(h, get_av_info_fn, "retro_get_system_av_info");
    void_fn init = SYM(h, void_fn, "retro_init");
    void_fn run = SYM(h, void_fn, "retro_run");
    void_fn deinit = SYM(h, void_fn, "retro_deinit");
    load_game_fn load = SYM(h, load_game_fn, "retro_load_game");
    assert(set_env && set_video && set_poll && set_state && get_av_info && init && run && deinit && load);
    struct retro_system_av_info av = {0};
    get_av_info(&av);
    assert(av.geometry.base_width == expected_width && av.geometry.base_height == expected_height);
    assert(av.geometry.max_width == expected_width && av.geometry.max_height == expected_height);
    assert(av.geometry.aspect_ratio == (float)expected_width / expected_height);
    set_env(environment); set_video(video); set_poll(poll); set_state(input); init();
    assert(load(NULL));
    run();
    tap(run, RETRO_DEVICE_ID_JOYPAD_A);
    for (unsigned i = 0; i < 3; ++i) run();
    tap(run, RETRO_DEVICE_ID_JOYPAD_B);
    for (unsigned i = 0; i < 3; ++i) run();

    typedef void (*test_route_fn)(const char *);
    test_route_fn test_route = SYM(h, test_route_fn, "retro_test_route");
    assert(test_route);
    typedef int (*test_assign_fn)(char *);
    test_assign_fn test_assign = SYM(h, test_assign_fn, "retro_test_auto_assign");
    assert(test_assign);
    static const char *const screens[] = {"collection", "history", "app", "info", "config"};
    for (unsigned screen = 0; screen < sizeof(screens) / sizeof(screens[0]); ++screen) {
        test_route(screens[screen]);
        run();
        test_route("launcher");
        run();
    }

    char browse_dir[512], browse_file[512];
    snprintf(browse_dir, sizeof browse_dir, "%sfixture-roms/FC", OPT_PATH);
    mkdir(OPT_PATH "fixture-roms", 0755); mkdir(browse_dir, 0755);
    snprintf(browse_file, sizeof browse_file, "%s/route-test.nes", browse_dir);
    FILE *browse = fopen(browse_file, "w"); assert(browse); fputs("route", browse); fclose(browse);
    FILE *context = fopen(EXPLORE_DIR, "w"); assert(context); fprintf(context, "%s\n", browse_dir); fclose(context);
    context = fopen(MUOS_HST_LOAD, "w"); assert(context); fputs("route-test.nes\n", context); fclose(context);
    context = fopen(MUOS_ASS_LOAD, "w"); assert(context);
    fprintf(context, "route-test.nes\n%s\nfc\n0\n0\n", browse_dir); fclose(context);
    context = fopen(ORDER_DIR_FROM, "w"); assert(context); fprintf(context, "%s\n", browse_dir); fclose(context);
    static const char *const nested[] = {"search", "assign", "tag", "option", "order"};
    for (unsigned route = 0; route < sizeof(nested) / sizeof(nested[0]); ++route) {
        test_route(nested[route]); run();
        test_route("launcher"); run();
    }

    typedef int (*test_value_fn)(void);
    test_value_fn selected_index = SYM(h, test_value_fn, "retro_test_selected_index");
    test_value_fn current_screen = SYM(h, test_value_fn, "retro_test_current_screen");
    assert(selected_index && current_screen);
    static const unsigned directions[] = {
        RETRO_DEVICE_ID_JOYPAD_UP, RETRO_DEVICE_ID_JOYPAD_DOWN,
        RETRO_DEVICE_ID_JOYPAD_LEFT, RETRO_DEVICE_ID_JOYPAD_RIGHT,
    };
    unsigned char path[8][8] = {{0}}, path_length[8] = {0}, seen[8] = {0}, queue[8];
    unsigned head = 0, tail = 0;
    test_route("launcher"); run();
    const int start_index = selected_index();
    assert(start_index == 0);
    seen[start_index] = 1;
    queue[tail++] = (unsigned char)start_index;
    while (head < tail) {
        const unsigned from = queue[head++];
        for (unsigned direction = 0; direction < sizeof(directions) / sizeof(directions[0]); ++direction) {
            test_route("launcher"); run();
            for (unsigned step = 0; step < path_length[from]; ++step)
                tap(run, directions[path[from][step]]);
            assert(selected_index() == (int)from);
            tap(run, directions[direction]);
            const int to = selected_index();
            if (to < 0 || to >= 8 || seen[to]) continue;
            assert(path_length[from] < 7 && tail < 8);
            memcpy(path[to], path[from], path_length[from]);
            path[to][path_length[from]] = (unsigned char)direction;
            path_length[to] = path_length[from] + 1;
            seen[to] = 1;
            queue[tail++] = (unsigned char)to;
        }
    }
    for (unsigned index = 0; index < 8; ++index) assert(seen[index]);
    static const int screen_for_item[] = {1, 2, 3, 4, 5, 6};
    for (unsigned index = 0; index < sizeof(screen_for_item) / sizeof(screen_for_item[0]); ++index) {
        test_route("launcher"); run();
        int current = selected_index();
        while (current / 4 < (int)index / 4) { tap(run, RETRO_DEVICE_ID_JOYPAD_DOWN); current = selected_index(); }
        while (current / 4 > (int)index / 4) { tap(run, RETRO_DEVICE_ID_JOYPAD_UP); current = selected_index(); }
        while (current % 4 < (int)index % 4) { tap(run, RETRO_DEVICE_ID_JOYPAD_RIGHT); current = selected_index(); }
        while (current % 4 > (int)index % 4) { tap(run, RETRO_DEVICE_ID_JOYPAD_LEFT); current = selected_index(); }
        assert(current == (int)index);
        tap(run, RETRO_DEVICE_ID_JOYPAD_A);
        assert(current_screen() == screen_for_item[index]);
        tap(run, RETRO_DEVICE_ID_JOYPAD_B);
        assert(current_screen() == 0);
    }

    test_route("explore"); run();
    assert(current_screen() == 1);
    pressed_buttons = (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_SELECT) |
                      (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_START);
    run();
    pressed_buttons = 0;
    run();
    assert(current_screen() == 0);

    pressed_buttons = UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_DOWN;
    run();
    pressed_buttons = 0;
    run();
    assert(frames >= 2 && nonblack > 1000 && polls >= 3 && saw_changed_hash);

    FILE *action = fopen(MUOS_ACT_LOAD, "w"); assert(action); fputs("reboot\n", action); fclose(action);
    for (unsigned i = 0; i < 8; ++i) run();
    FILE *launch = fopen(MUOS_HOST_LAUNCH_FILE, "r"); assert(launch);
    char request[512] = {0};
    size_t request_size = fread(request, 1, sizeof request - 1, launch);
    assert(request_size > 0); fclose(launch);
    char expected[512];
    snprintf(expected, sizeof expected, "standalone\n%s\n\n", MUOS_HOST_REBOOT_SCRIPT);
    assert(strcmp(request, expected) == 0);

    action = fopen(MUOS_ACT_LOAD, "w"); assert(action); fputs("shutdown\n", action); fclose(action);
    for (unsigned i = 0; i < 8; ++i) run();
    assert(shutdown_requested);
    launch = fopen(MUOS_HOST_LAUNCH_FILE, "r"); assert(launch);
    memset(request, 0, sizeof request);
    request_size = fread(request, 1, sizeof request - 1, launch);
    assert(request_size > 0); fclose(launch);
    snprintf(expected, sizeof expected, "standalone\n%s\n\n", MUOS_HOST_SHUTDOWN_SCRIPT);
    assert(strcmp(request, expected) == 0);

    char rom_directory[512], rom[512], core[512];
    const char *assign_dir = getenv("MUOS_HOST_ASSIGN_DIR");
    assert(assign_dir && *assign_dir);
    snprintf(rom_directory, sizeof rom_directory, "%s", assign_dir);
    char card_root[512];
    snprintf(card_root, sizeof card_root, "%s", assign_dir);
    const size_t suffix_size = strlen("/FC");
    assert(strlen(card_root) > suffix_size);
    card_root[strlen(card_root) - suffix_size] = '\0';
    snprintf(rom, sizeof rom, "%s/game.nes", rom_directory);
    FILE *rom_file = fopen(rom, "w"); assert(rom_file); fputs("fixture", rom_file); fclose(rom_file);
    assert(test_assign(rom_directory));
    char folder_config[512];
    snprintf(folder_config, sizeof folder_config, "%s/FC/core.cfg", INFO_CON_PATH);
    FILE *assignment = fopen(folder_config, "r"); assert(assignment);
    char config_lines[4][256] = {{0}};
    for (unsigned i = 0; i < 4; ++i) assert(fgets(config_lines[i], sizeof config_lines[i], assignment));
    fclose(assignment);
    assert(strcmp(config_lines[0], "fceumm_libretro.so\n") == 0);
    assert(strcmp(config_lines[1], "frogui_fc\n") == 0);
    assert(strcmp(config_lines[2], "Nintendo NES-Famicom\n") == 0);
    snprintf(core, sizeof core, "%s/fceumm_libretro.so", MUOS_HOST_CORES_PATH);
    assert(access(core, R_OK) == 0);
    FILE *loader = fopen(MUOS_ROM_LOAD, "w"); assert(loader);
    fprintf(loader, "game\nlr-do-not-use\nfc\n\n0\n0\n%s/\nFC\ngame.nes\n", card_root);
    fclose(loader);
    shutdown_requested = false;
    test_route("explore"); run();
    assert(shutdown_requested);
    launch = fopen(MUOS_HOST_LAUNCH_FILE, "r"); assert(launch);
    memset(request, 0, sizeof request);
    request_size = fread(request, 1, sizeof request - 1, launch);
    assert(request_size > 0); fclose(launch);
    snprintf(expected, sizeof expected, "%s\n%s\n", core, rom);
    assert(strcmp(request, expected) == 0);
    loader = fopen(MUOS_ROM_LOAD, "w"); assert(loader);
    fprintf(loader, "game\nquicknes_libretro.so\nfc\nNintendo NES-Famicom\n0\n0\n%s/\nFC\ngame.nes\n", card_root);
    fclose(loader);
    typedef int (*test_launch_fn)(void);
    test_launch_fn test_launch = SYM(h, test_launch_fn, "retro_test_launch_game");
    assert(test_launch);
    shutdown_requested = false;
    assert(test_launch());
    assert(shutdown_requested);
    launch = fopen(MUOS_HOST_LAUNCH_FILE, "r"); assert(launch);
    memset(request, 0, sizeof request);
    request_size = fread(request, 1, sizeof request - 1, launch);
    assert(request_size > 0); fclose(launch);
    snprintf(core, sizeof core, "%s/quicknes_libretro.so", MUOS_HOST_CORES_PATH);
    snprintf(expected, sizeof expected, "%s\n%s\n", core, rom);
    assert(strcmp(request, expected) == 0);
    loader = fopen(MUOS_ROM_LOAD, "w"); assert(loader);
    fprintf(loader, "game\nlr-do-not-use\nfc\nNintendo NES-Famicom\n0\n0\n%s/\nFC\ngame.nes\n", card_root);
    fclose(loader);
    shutdown_requested = false;
    assert(test_launch());
    launch = fopen(MUOS_HOST_LAUNCH_FILE, "r"); assert(launch);
    memset(request, 0, sizeof request);
    request_size = fread(request, 1, sizeof request - 1, launch);
    assert(request_size > 0); fclose(launch);
    snprintf(core, sizeof core, "%s/fceumm_libretro.so", MUOS_HOST_CORES_PATH);
    snprintf(expected, sizeof expected, "%s\n%s\n", core, rom);
    assert(strcmp(request, expected) == 0);
    deinit(); dlclose(h);
    puts("libretro QEMU core passed (menu navigation, nested routes, assigned FrogUI core launch, power, MinUIfied)");
    return 0;
}
