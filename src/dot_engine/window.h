#ifndef DOT_WINDOW_H
#define DOT_WINDOW_H

#define DOT_VK_SURFACE RGFW_VK_SURFACE

typedef struct DOT_Window{
        RGFW_window *window;
}DOT_Window;

struct RendererBackend;
internal void dot_window_mouse_pos_callback(RGFW_window *window, i32 x, i32 y, float vecX, float vecY);
internal void dot_window_init(DOT_Window *window);
internal void dot_window_shutdown(DOT_Window *window);
internal void dot_window_create_surface(DOT_Window* window, struct RendererBackend *backend);
internal b8   dot_window_get_framebuffer_size(DOT_Window *window, i32 *w, i32 *h);
internal b8   dot_window_get_size(DOT_Window *window, i32 *w, i32 *h);
internal b8   dot_window_should_close(DOT_Window *window);
#endif // !DOT_WINDOW_H
