#include "renderer/vulkan/vk_helper.h"
// HACK: Create window here for now
float mouse_data[2] = {0.0f, 0.0f};

internal void
dot_window_mouse_pos_callback(RGFW_window *window, i32 x, i32 y, float vecX, float vecY) {
    RGFW_UNUSED(vecX);
    RGFW_UNUSED(vecY);
    // printf("mouse moved %i %i\n", x, y);
    float halfWidth = (float)(window->w / 2.0f);
    float halfHeight = (float)(window->h / 2.0f);
    mouse_data[0] = (float)(x - halfWidth) / halfWidth;
    mouse_data[1] = (float)(y - halfHeight) / halfHeight;
}

// TODO: RGFW_createWindow is doing malloc, override this
internal void
dot_window_init(DOT_Window *window){
    window->window = RGFW_createWindow("Super Final DOT Engine", 0, 0, 500, 500,
                                        RGFW_windowAllowDND | RGFW_windowCenter);

    RGFW_window_setExitKey(window->window, RGFW_escape);
    RGFW_setMousePosCallback(dot_window_mouse_pos_callback);
}

internal void
dot_window_destroy(DOT_Window *window){
        RGFW_window_close(window->window);
}

// TODO: May need to pass in backend base and upcast based on type to do per backend things
// Or just make specific functions and pass in proper parameters
internal void
dot_window_create_surface(DOT_Window* window, struct RendererBackend* backend){
    switch (backend->backend_kind){
        case RENDERER_BACKEND_VK:{
            RendererBackendVk* vk_ctx = renderer_backend_as_vk(backend);
            VK_CHECK(RGFW_window_createSurface_Vulkan(window->window, vk_ctx->instance, &vk_ctx->surface));
            break;
        }
        case RENDERER_BACKEND_NULL:
        case RENDERER_BACKEND_GL:
        case RENDERER_BACKEND_DX12:
            DOT_ERROR("Backend not implemented");
            break;
        default: DOT_ERROR("Backend not set");
            break;
    }
}

internal b8
dot_window_get_framebuffer_size(DOT_Window *window, i32 *w, i32 *h){
    DOT_WARNING("There does not seem to be way to query framebuffer size in RGFW. Returning window size");
    return RGFW_window_getSize(window->window, w, h);
}

internal b8
dot_window_get_size(DOT_Window *window, i32 *w, i32* h){
    return RGFW_window_getSize(window->window, w, h);
}

internal b8
dot_window_should_close(DOT_Window *window){
    RGFW_event event;
    while (RGFW_window_checkEvent(window->window, &event)) {
        if(event.type == RGFW_quit){
            break;
        }
    }
    return RGFW_window_shouldClose(window->window);
}
