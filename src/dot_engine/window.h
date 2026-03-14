#ifndef DOT_WINDOW_H
#define DOT_WINDOW_H

typedef struct DOT_Extent2D{
    i32 x,y;
}DOT_Extent2D;

typedef struct DOT_Window{
    RGFW_window *window;
}DOT_Window;

struct RendererBackend;
internal void dot_window_init(DOT_Window *window);
internal void dot_window_shutdown(DOT_Window *window);
internal void dot_window_create_surface(DOT_Window* window, struct RendererBackend *backend);
internal void dot_window_poll_events(DOT_Window *window);
internal b32  dot_window_should_close(DOT_Window *window);
internal DOT_Extent2D dot_window_get_framebuffer_size(DOT_Window *window);
internal DOT_Extent2D dot_window_get_size(DOT_Window *window);

#define DOT_VK_SURFACE RGFW_VK_SURFACE

#endif // !DOT_WINDOW_H
