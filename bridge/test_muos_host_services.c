#include "lvgl.h"
#include "common/ui/font.h"
#include "common/platform/sysinfo.h"
#include "common/platform/video.h"
#include "common/ui/image.h"
#include <ctype.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "mojibake/mojibake.h"

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
void load_overlay_image_sdl(void) {}
void load_wallpaper(lv_obj_t *screen, lv_group_t *group, lv_obj_t *image, int wall_type) {
    LV_UNUSED(screen); LV_UNUSED(group); LV_UNUSED(image); LV_UNUSED(wall_type);
}
char *load_static_image(lv_obj_t *screen, lv_group_t *group, int wall_type) {
    static char empty[] = "";
    LV_UNUSED(screen); LV_UNUSED(group); LV_UNUSED(wall_type);
    return empty;
}

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

int font_context_changed(void) { return 0; }
int theme_has_font(void) { return 0; }
void font_cache_clear(void) {}

static void apply_font_tree(lv_obj_t *obj) {
    lv_obj_set_style_text_font(obj, &lv_font_unscii_8, LV_PART_MAIN);
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(obj); ++i) apply_font_tree(lv_obj_get_child(obj, i));
}

void load_font_text(lv_obj_t *screen) { apply_font_tree(screen); }
void load_font_section(const char *section, lv_obj_t *obj) {
    LV_UNUSED(section);
    apply_font_tree(obj);
}
