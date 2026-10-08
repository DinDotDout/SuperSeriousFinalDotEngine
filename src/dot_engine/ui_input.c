internal void
ui_input_clipboard_paste(nk_handle usr, struct nk_text_edit *edit)
{
    DOT_UNUSED(usr);
    String8 text = dot_window_clipboard_get();
    if(text.size){
        nk_textedit_paste(edit, text.cstr, cast(int)text.size);
    }
}

internal void
ui_input_clipboard_copy(nk_handle usr, const char *text, int len)
{
    DOT_UNUSED(usr);
    if(!len) return;
    dot_window_clipboard_set(string8(cast(u8*)text, cast(u64)len));
}

internal void
ui_input_init(UI_Input *input, struct nk_context *ctx)
{
    MEMORY_ZERO_STRUCT(input);
    ctx->clip.copy     = ui_input_clipboard_copy;
    ctx->clip.paste    = ui_input_clipboard_paste;
    ctx->clip.userdata = nk_handle_ptr(0);
}

internal void
ui_input_update(UI_Input *input, struct nk_context *ctx, DOT_Window *window)
{
    b8 *keys = window->key_down;

    nk_input_begin(ctx);

    for (u32 i = 0; i < window->text_len; ++i)
        nk_input_unicode(ctx, window->text[i]);

    /* Keyboard state */
    nk_input_key(ctx, NK_KEY_DEL,            keys[DOT_KeyKind_Delete]);
    nk_input_key(ctx, NK_KEY_ENTER,          keys[DOT_KeyKind_Return]);
    nk_input_key(ctx, NK_KEY_TAB,            keys[DOT_KeyKind_Tab]);
    nk_input_key(ctx, NK_KEY_BACKSPACE,      keys[DOT_KeyKind_Backspace]);
    nk_input_key(ctx, NK_KEY_UP,             keys[DOT_KeyKind_Up]);
    nk_input_key(ctx, NK_KEY_DOWN,           keys[DOT_KeyKind_Down]);
    nk_input_key(ctx, NK_KEY_LEFT,           keys[DOT_KeyKind_Left]);
    nk_input_key(ctx, NK_KEY_RIGHT,          keys[DOT_KeyKind_Right]);
    nk_input_key(ctx, NK_KEY_SHIFT,          keys[DOT_KeyKind_ShiftL] || keys[DOT_KeyKind_ShiftR]);
    nk_input_key(ctx, NK_KEY_SCROLL_UP,      0);
    nk_input_key(ctx, NK_KEY_SCROLL_DOWN,    0);

    /* Ctrl combos */
    {
        b32 ctrl = keys[DOT_KeyKind_ControlL] || keys[DOT_KeyKind_ControlR];
        nk_input_key(ctx, NK_KEY_COPY,             ctrl && keys[DOT_KeyKind_C]);
        nk_input_key(ctx, NK_KEY_PASTE,            ctrl && keys[DOT_KeyKind_V]);
        nk_input_key(ctx, NK_KEY_CUT,              ctrl && keys[DOT_KeyKind_X]);
        nk_input_key(ctx, NK_KEY_TEXT_UNDO,        ctrl && keys[DOT_KeyKind_Z]);
        nk_input_key(ctx, NK_KEY_TEXT_REDO,        ctrl && keys[DOT_KeyKind_Y]);
        nk_input_key(ctx, NK_KEY_TEXT_SELECT_ALL,  ctrl && keys[DOT_KeyKind_A]);
        nk_input_key(ctx, NK_KEY_TEXT_WORD_LEFT,   ctrl && keys[DOT_KeyKind_Left]);
        nk_input_key(ctx, NK_KEY_TEXT_WORD_RIGHT,  ctrl && keys[DOT_KeyKind_Right]);
        nk_input_key(ctx, NK_KEY_TEXT_LINE_START,  keys[DOT_KeyKind_Home]);
        nk_input_key(ctx, NK_KEY_TEXT_LINE_END,    keys[DOT_KeyKind_End]);
    }

    /* Mouse position & buttons */
    {
        i32 mx = window->mouse_pos.x;
        i32 my = window->mouse_pos.y;
        nk_input_motion(ctx, mx, my);

        if (window->mouse_pressed[DOT_MouseButtonKind_Left]) {
            f64 now = cast(f64)platform_get_time_ns() / (cast(f64)TO_USEC(1));
            f64 dt = now - input->last_button_click;
            if (dt > 20.0 && dt < 400.0) {
                input->is_double_click_down = nk_true;
                input->double_click_pos = nk_vec2((f32)mx, (f32)my);
            }
            input->last_button_click = now;
        }else{
            input->is_double_click_down = nk_false;
        }

        nk_input_button(ctx, NK_BUTTON_DOUBLE, (int)input->double_click_pos.x,
                         (int)input->double_click_pos.y, cast(nk_bool)input->is_double_click_down);
        nk_input_button(ctx, NK_BUTTON_LEFT,   mx, my, window->mouse_down[DOT_MouseButtonKind_Left]);
        nk_input_button(ctx, NK_BUTTON_MIDDLE, mx, my, window->mouse_down[DOT_MouseButtonKind_Middle]);
        nk_input_button(ctx, NK_BUTTON_RIGHT,  mx, my, window->mouse_down[DOT_MouseButtonKind_Right]);
    }

    nk_input_scroll(ctx, nk_vec2(window->scroll.x, window->scroll.y));

    nk_input_end(ctx);
}
