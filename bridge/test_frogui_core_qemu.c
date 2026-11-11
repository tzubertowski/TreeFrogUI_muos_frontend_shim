#include <assert.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libretro.h"

typedef void (*set_environment_fn)(retro_environment_t);
typedef void (*set_video_fn)(retro_video_refresh_t);
typedef void (*set_audio_fn)(retro_audio_sample_t);
typedef void (*set_audio_batch_fn)(retro_audio_sample_batch_t);
typedef void (*set_poll_fn)(retro_input_poll_t);
typedef void (*set_state_fn)(retro_input_state_t);
typedef void (*void_fn)(void);
typedef bool (*load_game_fn)(const struct retro_game_info *);

static unsigned video_frames;

static bool environment(unsigned command, void *data) {
    if (command == RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME || command == RETRO_ENVIRONMENT_SET_PIXEL_FORMAT) return true;
    if (command == RETRO_ENVIRONMENT_GET_CAN_DUPE) {
        *(bool *)data = false;
        return true;
    }
    return false;
}

static void video(const void *data, unsigned width, unsigned height, size_t pitch) {
    assert(data && width > 0 && height > 0 && pitch >= width * 2);
    ++video_frames;
}

static void audio(int16_t left, int16_t right) { (void)left; (void)right; }
static size_t audio_batch(const int16_t *samples, size_t frames) { (void)samples; return frames; }
static void poll(void) {}
static int16_t input(unsigned port, unsigned device, unsigned index, unsigned id) {
    (void)port; (void)device; (void)index; (void)id;
    return 0;
}

#define SYM(handle, type, name) ((type)dlsym((handle), name))

int main(int argc, char **argv) {
    assert(argc == 2);
    char core_path[1024], rom_path[1024];
    FILE *launch = fopen(argv[1], "r"); assert(launch);
    assert(fgets(core_path, sizeof core_path, launch));
    assert(fgets(rom_path, sizeof rom_path, launch));
    fclose(launch);
    core_path[strcspn(core_path, "\r\n")] = '\0';
    rom_path[strcspn(rom_path, "\r\n")] = '\0';
    assert(strstr(core_path, "fceumm_libretro.so"));
    unsigned char rom[16 + 16384 + 8192] = {0};
    memcpy(rom, "NES\x1a", 4);
    rom[4] = 1;
    rom[5] = 1;
    memset(rom + 16, 0xea, 16384);
    rom[16 + 0x3ffa] = 0x00; rom[16 + 0x3ffb] = 0x80;
    rom[16 + 0x3ffc] = 0x00; rom[16 + 0x3ffd] = 0x80;
    rom[16 + 0x3ffe] = 0x00; rom[16 + 0x3fff] = 0x80;
    FILE *file = fopen(rom_path, "wb"); assert(file);
    assert(fwrite(rom, 1, sizeof rom, file) == sizeof rom);
    assert(fclose(file) == 0);

    void *core = dlopen(core_path, RTLD_NOW | RTLD_LOCAL);
    if (!core) { fprintf(stderr, "%s\n", dlerror()); return 2; }
    set_environment_fn set_environment = SYM(core, set_environment_fn, "retro_set_environment");
    set_video_fn set_video = SYM(core, set_video_fn, "retro_set_video_refresh");
    set_audio_fn set_audio = SYM(core, set_audio_fn, "retro_set_audio_sample");
    set_audio_batch_fn set_audio_batch = SYM(core, set_audio_batch_fn, "retro_set_audio_sample_batch");
    set_poll_fn set_poll = SYM(core, set_poll_fn, "retro_set_input_poll");
    set_state_fn set_state = SYM(core, set_state_fn, "retro_set_input_state");
    void_fn init = SYM(core, void_fn, "retro_init");
    void_fn run = SYM(core, void_fn, "retro_run");
    void_fn deinit = SYM(core, void_fn, "retro_deinit");
    load_game_fn load = SYM(core, load_game_fn, "retro_load_game");
    assert(set_environment && set_video && set_audio && set_audio_batch && set_poll && set_state && init && run && deinit && load);
    set_environment(environment); set_video(video); set_audio(audio); set_audio_batch(audio_batch);
    set_poll(poll); set_state(input); init();
    const struct retro_game_info game = {.path = rom_path, .data = rom, .size = sizeof rom};
    assert(load(&game));
    for (unsigned frame = 0; frame < 120; ++frame) run();
    assert(video_frames > 0);
    deinit(); dlclose(core);
    puts("FrogUI launch file booted its FCEUmm NES core under QEMU");
    return 0;
}
