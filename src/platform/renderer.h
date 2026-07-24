#ifndef PLATFORM_RENDERER_H
#define PLATFORM_RENDERER_H

#include "graphics/color.h"

int platform_renderer_init(void *window);

int platform_renderer_create_render_texture(int width, int height);

int platform_renderer_lost_render_texture(void);

void platform_renderer_invalidate_target_textures(void);

void platform_renderer_generate_mouse_cursor_texture(int cursor_id, int size, const color_t *pixels,
    int hotspot_x, int hotspot_y);

void platform_renderer_clear(void);

void platform_renderer_render(void);

/**
 * Convert window pixel coordinates to renderer logical coordinates.
 * Required so mouse input matches the hardware cursor under display scale.
 * @param window_x Window X in pixels
 * @param window_y Window Y in pixels
 * @param logical_x Output logical X
 * @param logical_y Output logical Y
 */
void platform_renderer_window_to_logical(int window_x, int window_y, float *logical_x, float *logical_y);

void platform_renderer_pause(void);

void platform_renderer_resume(void);

void platform_renderer_destroy(void);

#endif // PLATFORM_RENDERER_H
