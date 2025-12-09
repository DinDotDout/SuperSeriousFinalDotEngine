#include "renderer/vulkan/vk_helper.h"
// HACK: Create window here for now
float mouse_data[2] = {0.0f, 0.0f};
void DOT_Window_MousePosCallback(RGFW_window *window, i32 x, i32 y, float vecX, float vecY) {
    RGFW_UNUSED(vecX);
    RGFW_UNUSED(vecY);
    // printf("mouse moved %i %i\n", x, y);
    float halfWidth = (float)(window->w / 2.0f);
    float halfHeight = (float)(window->h / 2.0f);
    mouse_data[0] = (float)(x - halfWidth) / halfWidth;
    mouse_data[1] = (float)(y - halfHeight) / halfHeight;
}

// TODO: RGFW_createWindow is doing malloc, override this
internal void DOT_Window_Init(DOT_Window* window){
    window->window = RGFW_createWindow("Super Final DOT Engine", 0, 0, 500, 500,
                                        RGFW_windowAllowDND | RGFW_windowCenter);

    RGFW_window_setExitKey(window->window, RGFW_escape);
    RGFW_setMousePosCallback(DOT_Window_MousePosCallback);
}

internal void DOT_Window_Destroy(DOT_Window* window){
        RGFW_window_close(window->window);
}

// TODO: May need to pass in backend base and upcast based on type to do per backend things
// Or just make specific functions and pass in proper parameters
internal void DOT_Window_CreateSurface(DOT_Window* window, DOT_RendererBackendBase* backend){
    switch (backend->backend_kind) {
        case DOT_RENDERER_BACKEND_VK:
            DOT_RendererBackendVk* vk_ctx = DOT_RendererBackendBase_AsVk(backend);
            VkCheck(RGFW_window_createSurface_Vulkan(window->window, vk_ctx->instance, &vk_ctx->surface));
            break;
        case DOT_RENDERER_BACKEND_GL:
        case DOT_RENDERER_BACKEND_DX12:
            DOT_ERROR("Backend not implemented");
            break;
        default: DOT_ERROR("Backend not set");
            break;
    }
}

internal b8 DOT_Window_ShouldClose(DOT_Window* window){
    RGFW_event event;
    while (RGFW_window_checkEvent(window->window, &event)) {
        // if (event.type == RGFW_quit) {
        //     break;
        // }
    }
    return RGFW_window_shouldClose(window->window);
}

// TODO: Should we eventually return more than one capability
internal inline const char* DOT_Window_GetCapabilities(){
    return RGFW_VK_SURFACE;
}
