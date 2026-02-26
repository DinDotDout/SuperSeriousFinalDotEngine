float mouse_data[2] = {0.0f, 0.0f};
internal void
dot_window_mouse_pos_callback(RGFW_window *window, i32 x, i32 y, float vecX, float vecY)
{
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
dot_window_init(DOT_Window *window)
{
    window->window = RGFW_createWindow("Super Final DOT Engine", 0, 0, 500, 500,
                                        RGFW_windowAllowDND | RGFW_windowCenter);

    RGFW_window_setExitKey(window->window, RGFW_escape);
    // RGFW_setMousePosCallback(dot_window_mouse_pos_callback);
}

internal void
dot_window_shutdown(DOT_Window *window)
{
        RGFW_window_close(window->window);
}

internal void
dot_window_create_surface(DOT_Window* window, struct RendererBackend* backend)
{
    switch (backend->backend_kind){
    case RendererBackendKind_Vk:{
        RendererBackendVk* vk_ctx = renderer_backend_as_vk(backend);
        VkResult res = RGFW_window_createSurface_Vulkan(window->window, vk_ctx->instance, &vk_ctx->surface);
        if(res != VK_SUCCESS){
            DOT_ERROR("Could not create vulkan surface");
        }
    break;}
    case RendererBackendKind_Null: break;
    case RendererBackendKind_Dx12: DOT_ERROR("Dx12 Backend not implemented"); break;
    default: DOT_ERROR("Backend not set"); break;
    }
}

internal b8
dot_window_get_framebuffer_size(DOT_Window *window, i32 *w, i32 *h)
{
    DOT_WARNING("There does not seem to be way to query framebuffer size in RGFW. Returning window size");
    return RGFW_window_getSize(window->window, w, h);
}

internal b8
dot_window_get_size(DOT_Window *window, i32 *w, i32* h)
{
    return RGFW_window_getSize(window->window, w, h);
}

internal b8
dot_window_should_close(DOT_Window *window)
{
    RGFW_event event;
    while (RGFW_window_checkEvent(window->window, &event)) {
    }

    return RGFW_window_shouldClose(window->window);
}
