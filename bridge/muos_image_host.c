#include <stdint.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "lvgl.h"
#include "muos_host_diag.h"

#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_GIF
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "../upstream/vendor/plutosvg/plutovg/source/plutovg-stb-image.h"

/* shortcut: GIFs use their first frame; add timing when animated muOS artwork is supported. */

static atomic_uint image_diag_count;
static void image_diag(const char *event, const char *path, const char *reason, int width, int height) {
    if (atomic_fetch_add(&image_diag_count, 1) < 32)
        muos_diag_log(event, "path=%s size=%dx%d reason=%s", path ? path : "(null)", width, height,
                      reason ? reason : "ok");
}

static const char *image_path(const void *src) {
    if (!src || lv_img_src_get_type(src) != LV_IMG_SRC_FILE) return NULL;
    const char *path = src;
    return path[0] && path[1] == ':' ? path + 2 : path;
}

static lv_res_t image_info(lv_img_decoder_t *decoder, const void *src, lv_img_header_t *header) {
    LV_UNUSED(decoder);
    const char *path = image_path(src);
    int width = 0, height = 0, channels = 0;
    if (!path) return LV_RES_INV;
    if (!stbi_info(path, &width, &height, &channels) || width <= 0 || height <= 0 ||
        width > 2047 || height > 2047 || (int64_t)width * height > 4 * 1024 * 1024) {
        image_diag("image_info_failed", path, stbi_failure_reason(), width, height);
        return LV_RES_INV;
    }

    header->always_zero = 0;
    header->cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
    header->w = width;
    header->h = height;
    return LV_RES_OK;
}

static lv_res_t image_open(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc) {
    LV_UNUSED(decoder);
    const char *path = image_path(dsc->src);
    int width = 0, height = 0, channels = 0;
    stbi_uc *pixels = path ? stbi_load(path, &width, &height, &channels, STBI_rgb_alpha) : NULL;
    if (!pixels || width <= 0 || height <= 0 || width > 2047 || height > 2047 ||
        (int64_t)width * height > 4 * 1024 * 1024) {
        image_diag("image_decode_failed", path, stbi_failure_reason(), width, height);
        stbi_image_free(pixels);
        return LV_RES_INV;
    }
    image_diag("image_decoded", path, NULL, width, height);

    const size_t count = (size_t)width * (size_t)height;
    for (size_t i = 0; i < count; ++i) {
        uint8_t *pixel = pixels + i * 4;
        const uint8_t red = pixel[0];
        pixel[0] = pixel[2];
        pixel[2] = red;
    }

    dsc->header.always_zero = 0;
    dsc->header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
    dsc->header.w = width;
    dsc->header.h = height;
    dsc->img_data = pixels;
    dsc->user_data = pixels;
    return LV_RES_OK;
}

static void image_close(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc) {
    LV_UNUSED(decoder);
    stbi_image_free(dsc->user_data);
    dsc->user_data = NULL;
    dsc->img_data = NULL;
}

void img_init(void) {
    lv_img_decoder_t *decoder = lv_img_decoder_create();
    if (!decoder) {
        muos_diag_log("image_decoder", "registration failed");
        return;
    }
    lv_img_decoder_set_info_cb(decoder, image_info);
    lv_img_decoder_set_open_cb(decoder, image_open);
    lv_img_decoder_set_close_cb(decoder, image_close);
    muos_diag_log("image_decoder", "registered png,jpeg,bmp,gif");
}

static SDL_Surface *surface_from_rgba(int width, int height, uint8_t *pixels) {
    SDL_Surface *surface = calloc(1, sizeof(*surface));
    SDL_PixelFormat *format = calloc(1, sizeof(*format));
    if (!surface || !format) {
        free(surface);
        free(format);
        stbi_image_free(pixels);
        return NULL;
    }

    format->format = SDL_PIXELFORMAT_RGBA32;
    format->BitsPerPixel = 32;
    format->BytesPerPixel = 4;
    surface->format = format;
    surface->w = width;
    surface->h = height;
    surface->pitch = width * 4;
    surface->pixels = pixels;
    surface->refcount = 1;
    return surface;
}

SDL_Surface *IMG_Load(const char *file) {
    struct stat st;
    if (!file || stat(file, &st) != 0 || st.st_size <= 0 || st.st_size > 32 * 1024 * 1024) {
        image_diag("sdl_image_failed", file, "stat/size rejected", 0, 0);
        return NULL;
    }
    int width = 0, height = 0, channels = 0;
    stbi_uc *pixels = stbi_load(file, &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels || width <= 0 || height <= 0 || width > 8192 || height > 8192 ||
        (int64_t)width * height > 16 * 1024 * 1024) {
        image_diag("sdl_image_failed", file, stbi_failure_reason(), width, height);
        stbi_image_free(pixels);
        return NULL;
    }
    image_diag("sdl_image_loaded", file, NULL, width, height);
    return surface_from_rgba(width, height, pixels);
}

SDL_Surface *SDL_ConvertSurfaceFormat(SDL_Surface *src, Uint32 format, Uint32 flags) {
    LV_UNUSED(flags);
    if (!src || !src->pixels || !src->format || src->format->BytesPerPixel != 4 ||
        (format != SDL_PIXELFORMAT_RGBA32 && format != SDL_PIXELFORMAT_BGRA32))
        return NULL;

    const size_t bytes = (size_t)src->w * (size_t)src->h * 4;
    uint8_t *pixels = malloc(bytes);
    if (!pixels) return NULL;
    const uint8_t *input = src->pixels;
    for (int y = 0; y < src->h; ++y) {
        const uint8_t *row = input + (size_t)y * (size_t)src->pitch;
        uint8_t *out = pixels + (size_t)y * (size_t)src->w * 4;
        for (int x = 0; x < src->w; ++x) {
            const uint8_t *pixel = row + (size_t)x * 4;
            uint8_t *converted = out + (size_t)x * 4;
            if (format == SDL_PIXELFORMAT_BGRA32) {
                converted[0] = pixel[2]; converted[1] = pixel[1];
                converted[2] = pixel[0]; converted[3] = pixel[3];
            } else {
                memcpy(converted, pixel, 4);
            }
        }
    }

    SDL_Surface *surface = surface_from_rgba(src->w, src->h, pixels);
    if (surface) surface->format->format = format;
    return surface;
}

void SDL_FreeSurface(SDL_Surface *surface) {
    if (!surface) return;
    free(surface->pixels);
    free(surface->format);
    free(surface);
}
