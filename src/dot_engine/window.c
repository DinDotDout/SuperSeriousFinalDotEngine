DOT_SETTING_U32("Window", dot_g_window_mode, DOT_WindowModeKind_Windowed);

internal read_only RGFW_key dot_window_rgfw_keys[DOT_KeyKind_Count] = {
    [DOT_KeyKind_Delete]    = RGFW_keyDelete,
    [DOT_KeyKind_Return]    = RGFW_keyReturn,
    [DOT_KeyKind_Tab]       = RGFW_keyTab,
    [DOT_KeyKind_Backspace] = RGFW_keyBackSpace,
    [DOT_KeyKind_Up]        = RGFW_keyUp,
    [DOT_KeyKind_Down]      = RGFW_keyDown,
    [DOT_KeyKind_Left]      = RGFW_keyLeft,
    [DOT_KeyKind_Right]     = RGFW_keyRight,
    [DOT_KeyKind_Home]      = RGFW_keyHome,
    [DOT_KeyKind_End]       = RGFW_keyEnd,
    [DOT_KeyKind_ShiftL]    = RGFW_keyShiftL,
    [DOT_KeyKind_ShiftR]    = RGFW_keyShiftR,
    [DOT_KeyKind_ControlL]  = RGFW_keyControlL,
    [DOT_KeyKind_ControlR]  = RGFW_keyControlR,
    [DOT_KeyKind_A]         = RGFW_keyA,
    [DOT_KeyKind_C]         = RGFW_keyC,
    [DOT_KeyKind_V]         = RGFW_keyV,
    [DOT_KeyKind_X]         = RGFW_keyX,
    [DOT_KeyKind_Y]         = RGFW_keyY,
    [DOT_KeyKind_Z]         = RGFW_keyZ,
};

internal read_only RGFW_mouseButton dot_window_rgfw_mouse_buttons[DOT_MouseButtonKind_Count] = {
    [DOT_MouseButtonKind_Left]   = RGFW_mouseLeft,
    [DOT_MouseButtonKind_Middle] = RGFW_mouseMiddle,
    [DOT_MouseButtonKind_Right]  = RGFW_mouseRight,
};

internal void
dot_window_init(DOT_Window *window, String8 window_name)
{
    i32 init_result = RGFW_init(window_name.cstr, 0);
    DOT_ASSERT(init_result >= 0, "RGFW_init failed (%d)", init_result);
    RGFW_windowFlags flags = RGFW_windowAllowDND;
    switch(cast(DOT_WindowModeKind) dot_g_window_mode){
    case DOT_WindowModeKind_Fullscreen: flags |= RGFW_windowFullscreen; break;
    case DOT_WindowModeKind_Maximized:  flags |= RGFW_windowMaximize;   break;
    case DOT_WindowModeKind_Windowed:
#if !defined(RGFW_WAYLAND)
    // Centering reads the primary monitor, and upstream RGFW has no monitors on Wayland yet.
    // Wayland compositors place windows themselves anyway.
    flags |= RGFW_windowCenter;
#endif
    break;
    default: DOT_WARNING("Unknown window mode %u", dot_g_window_mode); break;
    }
    window->window = RGFW_createWindow("Super Final DOT Engine", 0, 0, 500, 500, flags);
    RGFW_window_setExitKey(window->window, RGFW_keyEscape);
}

internal void
dot_window_shutdown(DOT_Window *window)
{
    RGFW_window_close(window->window);
    RGFW_deinit();
}

internal i32x2
dot_window_get_framebuffer_size(DOT_Window *window)
{
    // Without a monitor RGFW has no pixel ratio (and asking dereferences a null monitor), which
    // happens on Wayland because upstream drops the outputs at init: assume a ratio of 1.
    i32x2 e = {0};
    size_t monitor_count = 0;
    RGFW_getMonitorsPtr(0, NULL, &monitor_count);
    if(monitor_count == 0 || !RGFW_window_getSizeInPixels(window->window, &e.x, &e.y)){
        RGFW_window_getSize(window->window, &e.x, &e.y);
    }
    return e;
}

internal i32x2
dot_window_get_size(DOT_Window *window)
{
    i32x2 e = {0};
    RGFW_window_getSize(window->window, &e.x, &e.y);
    return e;
}

internal void
dot_window_poll_events(DOT_Window *window)
{
    RGFW_event event;
    window->mouse_delta = (f32x2){0};
    window->scroll      = (f32x2){0};
    window->text_len    = 0;
    while(RGFW_window_checkEvent(window->window, &event)){
        if(event.type == RGFW_mouseScroll){
            window->scroll.x += event.delta.x;
            window->scroll.y += event.delta.y;
        }
        if(event.type == RGFW_keyChar && window->text_len < DOT_WINDOW_TEXT_MAX){
            window->text[window->text_len++] = event.keyChar.value;
        }
        if(event.type == RGFW_mouseRawMotion){
            window->mouse_delta.x += event.delta.x;
            window->mouse_delta.y += event.delta.y;
        }
        // RGFW's built-in exit key relies on RGFW_window_isKeyDown, which is
        // gated behind RGFW_window_isInFocus -- on this Wayland backend that
        // focus flag isn't being set reliably, so the exit key silently never
        // fires. React to the raw key press instead: it only depends on the
        // event actually reaching us, not on RGFW's internal focus bookkeeping.
        if(event.type == RGFW_keyPressed && event.key.value == RGFW_keyEscape){
            RGFW_window_setShouldClose(window->window, RGFW_TRUE);
        }
    }

    for(u32 i = 0; i < DOT_KeyKind_Count; ++i){
        window->key_down[i] = cast(b8)RGFW_isKeyDown(dot_window_rgfw_keys[i]);
    }
    for(u32 i = 0; i < DOT_MouseButtonKind_Count; ++i){
        window->mouse_down[i]    = cast(b8)RGFW_isMouseDown(dot_window_rgfw_mouse_buttons[i]);
        window->mouse_pressed[i] = cast(b8)RGFW_isMousePressed(dot_window_rgfw_mouse_buttons[i]);
    }
    RGFW_window_getMouse(window->window, &window->mouse_pos.x, &window->mouse_pos.y);
}

internal b32
dot_window_should_close(DOT_Window *window)
{
    return RGFW_window_shouldClose(window->window) == true;
}

internal PlatformNativeWindow
dot_window_native_get(DOT_Window *window)
{
    PlatformNativeWindow native = {0};
#if DOT_OS_WINDOWS
    native.kind   = PlatformWindowSystemKind_Win32;
    native.window = RGFW_window_getHWND(window->window);
#else
    if(RGFW_usingWayland()){
        native.kind    = PlatformWindowSystemKind_Wayland;
        native.display = RGFW_getDisplay_Wayland();
        native.window  = RGFW_window_getWindow_Wayland(window->window);
    }else{
        native.kind    = PlatformWindowSystemKind_Xlib;
        native.display = RGFW_getDisplay_X11();
        native.window  = cast(void*) RGFW_window_getWindow_X11(window->window);
    }
#endif
    return(native);
}

internal String8
dot_window_clipboard_get(void)
{
    String8 result = {0};
    const RGFW_dataTransfer *clip = RGFW_readClipboardString();
    if(clip && clip->data && clip->length){
        usize len = clip->length;
        if(clip->data[len - 1] == '\0') len -= 1;
        result = string8(cast(u8*)clip->data, cast(u64)len);
    }
    return(result);
}

internal void
dot_window_clipboard_set(String8 text)
{
    if(!text.size) return;
    RGFW_dataTransfer clip = { .data = text.cstr, .length = cast(size_t)text.size, .type = RGFW_dataText };
    RGFW_writeClipboard(&clip);
}
