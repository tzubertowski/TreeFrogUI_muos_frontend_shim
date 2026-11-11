#include "muos_host_supervisor.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "common/base/options.h"
#include "muos_host_diag.h"

static const char *frogui_core(const char *system, const char *filename) {
    static const struct { const char *system; const char *core; } systems[] = {
        {"FC", "fceumm"}, {"NES", "fceumm"}, {"nes", "fceumm"}, {"nesq", "quicknes"},
        {"nest", "nestopia"}, {"fds", "fceumm"}, {"SFC", "snes9x2005_plus"}, {"snes", "snes9x2005_plus"},
        {"snes02", "snes9x2002"}, {"snesz", "snes9x-Z"}, {"zsnes", "snes9x-Z"},
        {"GB", "gambatte"}, {"GBC", "gambatte"}, {"gb", "gambatte"}, {"gbc", "gambatte"},
        {"gbgb", "gearboy"}, {"gbb", "tgbdual"}, {"GBA", "gpsp"}, {"gba", "gpsp"},
        {"gbac", "gpsp_multicore"}, {"gbav", "vba_next"}, {"mgba", "mgba"}, {"gbaf", "mgba"},
        {"MD", "picodrive"}, {"sega", "picodrive"}, {"32x", "picodrive"}, {"SMS", "picodrive"},
        {"gg", "gearsystem"}, {"gpgx", "genesis_plus_gx"}, {"segacd", "genesis_plus_gx"},
        {"PS", "pcsx_rearmed"}, {"ps1r", "pcsx_rearmed"}, {"PSP", "ppsspp"}, {"psp", "ppsspp"},
        {"PCE", "mednafen_pce_fast"}, {"NGP", "race"}, {"NGPC", "race"},
        {"WS", "mednafen_wswan"}, {"WSC", "mednafen_wswan"}, {"MAME", "mame2000"},
    };
    const char *folder = strrchr(system, '/');
    folder = folder ? folder + 1 : system;
    for (size_t i = 0; i < sizeof(systems) / sizeof(systems[0]); ++i)
        if (strcasecmp(folder, systems[i].system) == 0) return systems[i].core;

    const char *extension = strrchr(filename, '.');
    if (!extension) return NULL;
    static const struct { const char *extension; const char *core; } extensions[] = {
        {".nes", "fceumm"}, {".fds", "fceumm"}, {".sfc", "snes9x2005_plus"}, {".smc", "snes9x2005_plus"},
        {".gb", "gambatte"}, {".gbc", "gambatte"}, {".gba", "gpsp"}, {".md", "picodrive"},
        {".smd", "picodrive"}, {".gen", "picodrive"}, {".sms", "picodrive"}, {".gg", "gearsystem"},
        {".cue", "pcsx_rearmed"}, {".iso", "pcsx_rearmed"}, {".pce", "mednafen_pce_fast"},
        {".ngp", "race"}, {".ngc", "race"}, {".ws", "mednafen_wswan"}, {".wsc", "mednafen_wswan"},
    };
    for (size_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); ++i)
        if (strcasecmp(extension, extensions[i].extension) == 0) return extensions[i].core;
    return NULL;
}

static void trim_line(char *line) {
    line[strcspn(line, "\r\n")] = '\0';
}

static int safe_relative_path(const char *path) {
    if (!path || !*path || *path == '/') return 0;
    for (const char *part = path; *part;) {
        while (*part == '/') ++part;
        const char *end = part;
        while (*end && *end != '/') ++end;
        if ((end - part == 2 && part[0] == '.' && part[1] == '.') || end == part) return 0;
        part = end;
    }
    return 1;
}

static const char *assigned_libretro_core(const char *stored) {
    static char filename[PATH_MAX];
    if (!stored || !*stored || strchr(stored, '/') || strchr(stored, '\\')) return NULL;
    const size_t length = strlen(stored);
    static const char suffix[] = "_libretro.so";
    if (length <= sizeof(suffix) - 1 || strcmp(stored + length - (sizeof(suffix) - 1), suffix) != 0)
        return NULL;
    for (const unsigned char *p = (const unsigned char *)stored; *p; ++p)
        if (!(isalnum(*p) || *p == '_' || *p == '-' || *p == '.')) return NULL;
    snprintf(filename, sizeof(filename), "%s", stored);
    return filename;
}

static int request_game(retro_environment_t environment, const char *core, const char *rom) {
    char temporary[sizeof(MUOS_HOST_LAUNCH_FILE) + sizeof(".tmp")];
    const int length = snprintf(temporary, sizeof(temporary), "%s.tmp", MUOS_HOST_LAUNCH_FILE);
    if (length < 0 || (size_t)length >= sizeof(temporary)) return 0;
    FILE *file = fopen(temporary, "w");
    if (!file) return 0;
    const int written = fprintf(file, "%s\n%s\n", core, rom);
    const int flushed = fflush(file);
    const int synced = fsync(fileno(file));
    const int closed = fclose(file);
    if (written < 0 || flushed != 0 || synced != 0 || closed != 0 ||
        rename(temporary, MUOS_HOST_LAUNCH_FILE) != 0) {
        unlink(temporary);
        return 0;
    }
    unlink(MUOS_ROM_LOAD);
    unlink(MUOS_ACT_LOAD);
    sync();
    muos_diag_log("launch", "game core=%s rom=%s", core, rom);
    if (environment) environment(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
    return 1;
}

int muos_host_launch_game(retro_environment_t environment) {
    FILE *file = fopen(MUOS_ROM_LOAD, "r");
    if (!file) return 0;
    char lines[9][PATH_MAX] = {{0}};
    int valid = 1;
    for (size_t i = 0; i < 9; ++i) {
        if (!fgets(lines[i], sizeof(lines[i]), file)) { valid = 0; break; }
        trim_line(lines[i]);
    }
    fclose(file);
    if (!valid || lines[6][0] != '/' || !safe_relative_path(lines[7]) || !safe_relative_path(lines[8])) {
        muos_diag_log("launch", "invalid MuOS content loader");
        return 0;
    }

    const char *core_file = assigned_libretro_core(lines[1]);
    char fallback_core[PATH_MAX];
    if (core_file) {
        snprintf(fallback_core, sizeof(fallback_core), "%s", core_file);
    } else {
        const char *core_name = frogui_core(lines[7], lines[8]);
        if (!core_name) {
            muos_diag_log("launch", "no FrogUI core mapping system=%s file=%s", lines[7], lines[8]);
            return 0;
        }
        snprintf(fallback_core, sizeof(fallback_core), "%s_libretro.so", core_name);
    }
    char core[PATH_MAX], rom[PATH_MAX];
    if (snprintf(core, sizeof(core), "%s/%s", MUOS_HOST_CORES_PATH, fallback_core) >= (int)sizeof(core))
        return 0;
    size_t root_size = strlen(lines[6]);
    while (root_size > 1 && lines[6][root_size - 1] == '/') lines[6][--root_size] = '\0';
    const int rom_size = snprintf(rom, sizeof(rom), "%s/%s/%s", lines[6], lines[7], lines[8]);
    if (rom_size < 0 || (size_t)rom_size >= sizeof(rom) || access(core, R_OK) != 0 || access(rom, R_OK) != 0) {
        muos_diag_log("launch", "missing FrogUI core or ROM core=%s rom=%s", core, rom);
        return 0;
    }
    muos_diag_log("launch", "MuOS assigned core=%s", fallback_core);
    return request_game(environment, core, rom);
}

static int request_exit(retro_environment_t environment, const char *script) {
    char temporary[sizeof(MUOS_HOST_LAUNCH_FILE) + sizeof(".tmp")];
    const int length = snprintf(temporary, sizeof(temporary), "%s.tmp", MUOS_HOST_LAUNCH_FILE);
    if (length < 0 || (size_t)length >= sizeof(temporary)) return 0;

    FILE *file = fopen(temporary, "w");
    if (!file) return 0;
    const int written = fprintf(file, "standalone\n%s\n\n", script);
    const int flushed = fflush(file);
    const int synced = fsync(fileno(file));
    const int closed = fclose(file);
    if (written < 0 || flushed != 0 || synced != 0 || closed != 0 ||
        rename(temporary, MUOS_HOST_LAUNCH_FILE) != 0) {
        unlink(temporary);
        return 0;
    }

    unlink(MUOS_ACT_LOAD);
    sync();
    if (environment) environment(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
    return 1;
}

void muos_host_supervisor_step(retro_environment_t environment) {
    static unsigned poll_frames;
    if (++poll_frames < 8) return;
    poll_frames = 0;

    FILE *file = fopen(MUOS_ACT_LOAD, "r");
    if (!file) return;

    char action[32] = {0};
    const char *read = fgets(action, sizeof(action), file);
    fclose(file);
    if (!read) return;
    action[strcspn(action, "\r\n")] = '\0';

    if (strcmp(action, "shutdown") == 0) request_exit(environment, MUOS_HOST_SHUTDOWN_SCRIPT);
    else if (strcmp(action, "reboot") == 0) request_exit(environment, MUOS_HOST_REBOOT_SCRIPT);
}
