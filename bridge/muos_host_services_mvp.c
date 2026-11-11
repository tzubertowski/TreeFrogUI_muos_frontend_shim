#include "lvgl.h"
#include "common/ui/font.h"
#include "common/display/theme_base.h"
#include "common/platform/sysinfo.h"
#include "common/platform/video.h"
#include "common/ui/image.h"
#include "common/ui/nav/cell_art.h"
#include "common/platform/display.h"
#include "common/platform/device.h"
#include "common/platform/video.h"
#include "common/config/passcode.h"
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include "mojibake/mojibake.h"
#include "muos_host_diag.h"

/* Prototype host adapters: LVGL supplies time, while optional SDL/device
 * services stay inert until the libretro host gains equivalent callbacks. */

Uint32 SDL_GetTicks(void) { return lv_tick_get(); }
Uint64 SDL_GetPerformanceCounter(void) { return lv_tick_get(); }
Uint64 SDL_GetPerformanceFrequency(void) { return 1000; }
void get_network_snapshot(network_snapshot *snapshot, unsigned fields) {
    LV_UNUSED(fields);
    memset(snapshot, 0, sizeof(*snapshot));
}
int brightness_to_percent(int value) { return value; }
int volume_to_percent(int value) { return value; }
int video_preview_active(void) { return 0; }
void video_preview_cancel(void) {}
void video_preview_destroy(void) {}
int video_preview_matches(const char *path) { LV_UNUSED(path); return 0; }
void video_preview_arm(const char *path, int delay_ms, const lv_obj_t *container, const lv_obj_t *box_img) {
    LV_UNUSED(path); LV_UNUSED(delay_ms); LV_UNUSED(container); LV_UNUSED(box_img);
}
void video_preview_arm_image(const char *path, lv_obj_t *image, video_size_cb size) {
    LV_UNUSED(path); LV_UNUSED(image); LV_UNUSED(size);
}
void video_preview_arm_image_delay(const char *path, lv_obj_t *image, video_size_cb size, int delay_ms) {
    LV_UNUSED(path); LV_UNUSED(image); LV_UNUSED(size); LV_UNUSED(delay_ms);
}
void display_clear_theme_overlay(void) {}
SDL_Texture *display_load_image_texture(const char *path) { LV_UNUSED(path); return NULL; }
void display_set_theme_overlay(SDL_Texture *texture, uint8_t opacity) { LV_UNUSED(texture); LV_UNUSED(opacity); }

mjb_status mjb_map_case_into(const char *src, size_t size, int src_enc, int mode, int dst_enc, char *dst,
                             size_t *dst_size) {
    LV_UNUSED(src_enc); LV_UNUSED(mode); LV_UNUSED(dst_enc);
    if (*dst_size < size) return 1;
    for (size_t i = 0; i < size; ++i) dst[i] = (char)tolower((unsigned char)src[i]);
    *dst_size = size;
    return MJB_STATUS_OK;
}

mjb_status mjb_collation_compare(const char *a, size_t a_size, int a_enc, const char *b, size_t b_size, int b_enc,
                                 int strength, int level, int *order) {
    LV_UNUSED(a_enc); LV_UNUSED(b_enc); LV_UNUSED(strength); LV_UNUSED(level);
    const size_t size = a_size < b_size ? a_size : b_size;
    const int cmp = memcmp(a, b, size);
    *order = cmp ? cmp : (a_size > b_size) - (a_size < b_size);
    return MJB_STATUS_OK;
}

static lv_font_t *theme_font;

static int theme_font_path(char *path, size_t size) {
    if (!theme_base || !*theme_base) return 0;
    char resolved[PATH_MAX];
    snprintf(resolved, sizeof(resolved), "%s/%ux%u/font/default.bin", theme_base,
             (unsigned)device.screen.width, (unsigned)device.screen.height);
    if (access(resolved, R_OK) == 0) {
        snprintf(path, size, "M:%s/%ux%u/font/default.bin", theme_base,
                 (unsigned)device.screen.width, (unsigned)device.screen.height);
        return 1;
    }
    snprintf(path, size, "M:%s/font/default.bin", theme_base);
    return 1;
}

static lv_font_t *load_theme_font(void) {
    if (theme_font) return theme_font;
    char path[1024];
    if (theme_font_path(path, sizeof(path))) {
        theme_font = lv_font_load(path);
        muos_diag_log("theme_font", "%s path=%s line_height=%d",
                      theme_font ? "loaded" : "load_failed", path, theme_font ? theme_font->line_height : 0);
    }
    return theme_font;
}

int font_context_changed(void) { return 0; }
int theme_has_font(void) {
    char path[1024];
    struct stat info;
    if (!theme_font_path(path, sizeof(path))) return 0;
    const char *file = path[0] == 'M' && path[1] == ':' ? path + 2 : path;
    return stat(file, &info) == 0 && info.st_size > 0;
}
void font_cache_clear(void) {
    if (theme_font) lv_font_free(theme_font);
    theme_font = NULL;
}

static void apply_font_tree(lv_obj_t *obj) {
    lv_font_t *font = load_theme_font();
    lv_obj_set_style_text_font(obj, font ? font : LV_FONT_DEFAULT, LV_PART_MAIN);
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(obj); ++i) apply_font_tree(lv_obj_get_child(obj, i));
}

void load_font_text(lv_obj_t *screen) { apply_font_tree(screen); }
void load_font_section(const char *section, lv_obj_t *obj) {
    LV_UNUSED(section);
    apply_font_tree(obj);
}
void load_font_section_scaled(const char *section, lv_obj_t *obj, int percent) {
    LV_UNUSED(percent);
    load_font_section(section, obj);
}
lv_font_t *font_style_variant(const lv_font_t *base, int bold, int italic) {
    LV_UNUSED(bold); LV_UNUSED(italic);
    return (lv_font_t *)base;
}

void display_process_screenshot_request(void) {}
void display_set_fade_alpha(uint8_t alpha) { LV_UNUSED(alpha); }
int video_wallpaper_active(void) { return 0; }
void video_wallpaper_play(const char *path) { LV_UNUSED(path); }
void video_wallpaper_stop(void) {}
int is_bluetooth_connected(void) { return 0; }
const char *resolve_info_path(const char *rel) { return rel; }
void cell_art_apply(lv_obj_t *panel, const char *item, int w, int h, int slant, int focused, int native) {
    LV_UNUSED(panel); LV_UNUSED(item); LV_UNUSED(w); LV_UNUSED(h);
    LV_UNUSED(slant); LV_UNUSED(focused); LV_UNUSED(native);
}
void cell_art_item(lv_obj_t *panel, const char *item) { LV_UNUSED(panel); LV_UNUSED(item); }
void cell_art_resize(lv_obj_t *panel, int w, int h) { LV_UNUSED(panel); LV_UNUSED(w); LV_UNUSED(h); }
lv_opa_t cell_opa(const lv_obj_t *panel) { return lv_obj_get_style_opa(panel, LV_PART_MAIN); }
void cell_set_opa(lv_obj_t *panel, lv_opa_t opacity) { lv_obj_set_style_opa(panel, opacity, LV_PART_MAIN); }
void cell_art_forget(void) {}

const char *SDL_GetError(void) { return "SDL services unavailable in libretro host"; }
void display_composite_frame(void) {}
int display_startup_transition_active(void) { return 0; }
void display_finish_startup_transition(void) {}
int display_ui_is_hidden(void) { return 0; }
double display_panel_refresh_hz(void) { return 60.0; }
SDL_Renderer *display_get_renderer(void) { return NULL; }
SDL_mutex *SDL_CreateMutex(void) { return NULL; }
int SDL_LockMutex(SDL_mutex *mutex) { LV_UNUSED(mutex); return 0; }
int SDL_UnlockMutex(SDL_mutex *mutex) { LV_UNUSED(mutex); return 0; }
void SDL_DestroyMutex(SDL_mutex *mutex) { LV_UNUSED(mutex); }
SDL_Thread *SDL_CreateThread(SDL_ThreadFunction fn, const char *name, void *data) {
    LV_UNUSED(fn); LV_UNUSED(name); LV_UNUSED(data); return NULL;
}
void SDL_WaitThread(SDL_Thread *thread, int *status) { LV_UNUSED(thread); if (status) *status = 0; }
int SDL_PushEvent(SDL_Event *event) { LV_UNUSED(event); return 0; }
void SDL_DestroyTexture(SDL_Texture *texture) { LV_UNUSED(texture); }
void Mix_FreeMusic(Mix_Music *music) { LV_UNUSED(music); }
int Mix_HaltMusic(void) { return 0; }
Mix_Music *Mix_LoadMUS(const char *file) { LV_UNUSED(file); return NULL; }
int Mix_PlayChannel(int channel, Mix_Chunk *chunk, int loops) {
    LV_UNUSED(channel); LV_UNUSED(chunk); LV_UNUSED(loops); return -1;
}
int Mix_PlayMusic(Mix_Music *music, int loops) { LV_UNUSED(music); LV_UNUSED(loops); return -1; }
int Mix_PlayingMusic(void) { return 0; }
int Mix_VolumeMusic(int volume) { return volume; }

/* SDL's blocking input/device paths stay unavailable inside the libretro host. */
void SDL_FlushEvents(Uint32 min_type, Uint32 max_type) { LV_UNUSED(min_type); LV_UNUSED(max_type); }
int SDL_GameControllerAddMapping(const char *mapping) { LV_UNUSED(mapping); return -1; }
int SDL_GameControllerAddMappingsFromRW(SDL_RWops *rw, int free_src) {
    LV_UNUSED(rw); LV_UNUSED(free_src); return -1;
}
void SDL_GameControllerClose(SDL_GameController *controller) { LV_UNUSED(controller); }
int SDL_GameControllerEventState(int state) { LV_UNUSED(state); return SDL_DISABLE; }
SDL_Joystick *SDL_GameControllerGetJoystick(SDL_GameController *controller) { LV_UNUSED(controller); return NULL; }
char *SDL_GameControllerMapping(SDL_GameController *controller) { LV_UNUSED(controller); return NULL; }
const char *SDL_GameControllerName(SDL_GameController *controller) { LV_UNUSED(controller); return NULL; }
SDL_GameController *SDL_GameControllerOpen(int index) { LV_UNUSED(index); return NULL; }
const char *SDL_GetCurrentVideoDriver(void) { return NULL; }
int SDL_GetRendererInfo(SDL_Renderer *renderer, SDL_RendererInfo *info) {
    LV_UNUSED(renderer); LV_UNUSED(info); return -1;
}
const char *SDL_GetScancodeName(SDL_Scancode scancode) { LV_UNUSED(scancode); return ""; }
int SDL_InitSubSystem(Uint32 flags) { LV_UNUSED(flags); return -1; }
SDL_bool SDL_IsGameController(int index) { LV_UNUSED(index); return SDL_FALSE; }
void SDL_JoystickClose(SDL_Joystick *joystick) { LV_UNUSED(joystick); }
int SDL_JoystickEventState(int state) { LV_UNUSED(state); return SDL_DISABLE; }
SDL_JoystickGUID SDL_JoystickGetGUID(SDL_Joystick *joystick) {
    LV_UNUSED(joystick); SDL_JoystickGUID guid = {{0}}; return guid;
}
SDL_JoystickGUID SDL_JoystickGetGUIDFromString(const char *guid) {
    LV_UNUSED(guid); SDL_JoystickGUID value = {{0}}; return value;
}
void SDL_JoystickGetGUIDString(SDL_JoystickGUID guid, char *text, int size) {
    LV_UNUSED(guid); if (text && size > 0) text[0] = '\0';
}
SDL_JoystickID SDL_JoystickInstanceID(SDL_Joystick *joystick) { LV_UNUSED(joystick); return -1; }
const char *SDL_JoystickName(SDL_Joystick *joystick) { LV_UNUSED(joystick); return NULL; }
const char *SDL_JoystickNameForIndex(int index) { LV_UNUSED(index); return NULL; }
int SDL_JoystickNumAxes(SDL_Joystick *joystick) { LV_UNUSED(joystick); return 0; }
int SDL_JoystickNumButtons(SDL_Joystick *joystick) { LV_UNUSED(joystick); return 0; }
SDL_Joystick *SDL_JoystickOpen(int index) { LV_UNUSED(index); return NULL; }
void SDL_JoystickUpdate(void) {}
int SDL_NumJoysticks(void) { return 0; }
int SDL_PollEvent(SDL_Event *event) { LV_UNUSED(event); return 0; }
void SDL_PumpEvents(void) {}
void SDL_QuitSubSystem(Uint32 flags) { LV_UNUSED(flags); }
SDL_RWops *SDL_RWFromFile(const char *file, const char *mode) { LV_UNUSED(file); LV_UNUSED(mode); return NULL; }
int SDL_WaitEventTimeout(SDL_Event *event, int timeout) { LV_UNUSED(event); LV_UNUSED(timeout); return 0; }
void SDL_free(void *memory) { free(memory); }
char *get_storage_label(const char *path, const char *primary, const char *secondary, const char *external) {
    LV_UNUSED(path); LV_UNUSED(secondary); LV_UNUSED(external); return (char *)primary;
}

struct mux_passcode passcode;
int muxpass_main(int auth_type) { LV_UNUSED(auth_type); return 2; }
