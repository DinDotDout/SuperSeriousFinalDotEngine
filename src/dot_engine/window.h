#ifndef DOT_WINDOW_H
#define DOT_WINDOW_H

enum { DOT_WINDOW_TEXT_MAX = 64 };

#define DOT_WINDOW_MODE_KINDS(X) \
    X(DOT_WindowModeKind, Fullscreen) \
    X(DOT_WindowModeKind, Maximized) \
    X(DOT_WindowModeKind, Windowed)
DOT_ENUM_REFLECT_TYPED(i32, DOT_WindowModeKind, DOT_WINDOW_MODE_KINDS)

#define DOT_WINDOW_SYSTEM_KINDS(X) \
    X(DOT_WindowSystemKind, Default) \
    X(DOT_WindowSystemKind, X11) \
    X(DOT_WindowSystemKind, Wayland)
DOT_ENUM_REFLECT_TYPED(i32, DOT_WindowSystemKind, DOT_WINDOW_SYSTEM_KINDS)

typedef enum DOT_KeyKind{
    DOT_KeyKind_Delete,
    DOT_KeyKind_Return,
    DOT_KeyKind_Tab,
    DOT_KeyKind_Backspace,
    DOT_KeyKind_Up,
    DOT_KeyKind_Down,
    DOT_KeyKind_Left,
    DOT_KeyKind_Right,
    DOT_KeyKind_Home,
    DOT_KeyKind_End,
    DOT_KeyKind_ShiftL,
    DOT_KeyKind_ShiftR,
    DOT_KeyKind_ControlL,
    DOT_KeyKind_ControlR,
    DOT_KeyKind_A,
    DOT_KeyKind_C,
    DOT_KeyKind_V,
    DOT_KeyKind_X,
    DOT_KeyKind_Y,
    DOT_KeyKind_Z,
    DOT_KeyKind_Count,
}DOT_KeyKind;

typedef enum DOT_MouseButtonKind{
    DOT_MouseButtonKind_Left,
    DOT_MouseButtonKind_Middle,
    DOT_MouseButtonKind_Right,
    DOT_MouseButtonKind_Count,
}DOT_MouseButtonKind;

typedef struct DOT_Window
{
    RGFW_window *window;
    // Sum of this poll's raw mouse motion. RGFW_getMousef32xtor keeps only the last event.
    f32x2 mouse_delta;
    f32x2 scroll;
    u32  text[DOT_WINDOW_TEXT_MAX];
    u32  text_len;
    i32x2 mouse_pos;
    b8   key_down[DOT_KeyKind_Count];
    b8   mouse_down[DOT_MouseButtonKind_Count];
    b8   mouse_pressed[DOT_MouseButtonKind_Count];
}DOT_Window;

internal void   dot_window_set_system(i32 kind);
internal void   dot_window_init(DOT_Window *window, String8 window_name);
internal void   dot_window_shutdown(DOT_Window *window);
internal void   dot_window_poll_events(DOT_Window *window);
internal b32    dot_window_should_close(DOT_Window *window);
internal i32x2  dot_window_get_framebuffer_size(DOT_Window *window);
internal i32x2  dot_window_get_size(DOT_Window *window);
internal PlatformNativeWindow dot_window_native_get(DOT_Window *window);
internal String8 dot_window_clipboard_get(void);
internal void    dot_window_clipboard_set(String8 text);

#endif // !DOT_WINDOW_H
