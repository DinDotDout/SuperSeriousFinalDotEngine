/*
 * nk_dot_window.h - Nuklear compatibility layer for DOT Engine
 *
 * Bridges RGFW windowing with the DOT_Renderer overlay API.
 *
 * This file contains NO graphics-API-specific code.  All GPU work is
 * delegated through the renderer frontend (renderer_overlay_*).
 *
 * Provides:
 *   - nk_dot_init()        : Initialise NK context, bake font atlas, call backend
 *   - nk_dot_new_frame()   : Pump RGFW events into NK input and begin a new UI frame
 *   - nk_dot_render()      : Convert NK draw commands -> OverlayDrawList -> renderer
 *   - nk_dot_shutdown()    : Tear down NK context and call backend
 *   - nk_dot_ctx()         : Accessor for the nk_context pointer
 *
 * Usage (inside engine loop):
 *
 *   nk_dot_init(&renderer, &window);
 *   ...
 *   // each frame:
 *   nk_dot_new_frame(&window);
 *   { nk_begin(...); ... nk_end(...); }
 *   nk_dot_render(&renderer);
 *   ...
 *   nk_dot_shutdown(&renderer);
 */

#ifndef NK_DOT_WINDOW_H
#define NK_DOT_WINDOW_H

/* ------------------------------------------------------------------ */
/*  Configuration                                                     */
/* ------------------------------------------------------------------ */

#define NK_DOT_MAX_VERTEX_BUFFER  (512 * 1024)
#define NK_DOT_MAX_ELEMENT_BUFFER (128 * 1024)
#define NK_DOT_MAX_DRAW_CMDS      4096
#define NK_DOT_TEXT_MAX           1024

/* ------------------------------------------------------------------ */
/*  Internal state  (no GPU types)                                    */
/* ------------------------------------------------------------------ */

typedef struct NkDot_State {
    /* Windowing */
    DOT_Window *win;
    DOT_Renderer *renderer;
    u32 width, height;

    /* Nuklear core */
    struct nk_context    ctx;
    struct nk_font_atlas font_atlas;
    struct nk_buffer     cmds;
    struct nk_draw_null_texture tex_null;

    /* Input helpers */
    u32 text[NK_DOT_TEXT_MAX];
    u32 text_len;
    struct nk_vec2 scroll;
    f64 last_button_click;
    u32 is_double_click_down;
    struct nk_vec2 double_click_pos;

    /* CPU-side staging buffers for nk_convert output */
    u8 vertex_buf[NK_DOT_MAX_VERTEX_BUFFER];
    u8 index_buf[NK_DOT_MAX_ELEMENT_BUFFER];
} NkDot_State;

global NkDot_State g_nk_dot;

/* ------------------------------------------------------------------ */
/*  Clipboard                                                         */
/* ------------------------------------------------------------------ */

NK_INTERN void
nk_dot_clipboard_paste(nk_handle usr, struct nk_text_edit *edit)
{
    UNUSED(usr);
    usize len = 0;
    const char *text = RGFW_readClipboard(&len);
    if (text)
        nk_textedit_paste(edit, text, (int)len);
}

NK_INTERN void
nk_dot_clipboard_copy(nk_handle usr, const char *text, int len)
{
    UNUSED(usr);
    if (!len) return;
    RGFW_writeClipboard(text, len);
}

/* ================================================================== */
/*  PUBLIC API                                                        */
/* ================================================================== */

/*
 * Initialise the Nuklear bridge.
 *   renderer - the DOT_Renderer (must already be initialised)
 *   window   - the DOT_Window (wrapping RGFW)
 */
NK_API struct nk_context*
nk_dot_init(DOT_Renderer *renderer, DOT_Window *window)
{
    MEMORY_ZERO_STRUCT(&g_nk_dot);
    NkDot_State *s = &g_nk_dot;
    s->win      = window;
    s->renderer = renderer;

    DOT_Extent2D e = dot_window_get_size(window);
    s->width  = (u32)e.x;
    s->height = (u32)e.y;

    /* NK context */
    nk_init_default(&s->ctx, NULL);
    s->ctx.clip.copy     = nk_dot_clipboard_copy;
    s->ctx.clip.paste    = nk_dot_clipboard_paste;
    s->ctx.clip.userdata = nk_handle_ptr(0);
    nk_buffer_init_default(&s->cmds);

    /* Bake font atlas (CPU-side) */
    const void *atlas_data;
    int atlas_w, atlas_h;
    nk_font_atlas_init_default(&s->font_atlas);
    nk_font_atlas_begin(&s->font_atlas);
    struct nk_font *font = nk_font_atlas_add_default(&s->font_atlas, 14.0f, NULL);
    atlas_data = nk_font_atlas_bake(&s->font_atlas, &atlas_w, &atlas_h, NK_FONT_ATLAS_RGBA32);
    /* Delegate GPU resource creation + font upload to the renderer */
    renderer_overlay_init(renderer, atlas_data, atlas_w, atlas_h);

    /* Finish the atlas – use a dummy texture handle (backend owns the real one) */
    nk_font_atlas_end(&s->font_atlas, nk_handle_id(1), &s->tex_null);
    // nk_style_set_font(&s->ctx, &s->font_atlas.default_font->handle);
    // if (s->font_atlas.default_font)
    nk_style_set_font(&s->ctx, &font->handle);
        // nk_style_set_font(&s->ctx, &s->font_atlas.default_font->handle);

    return &s->ctx;
}

/*
 * Get the nk_context (for issuing draw commands).
 */
NK_API struct nk_context*
nk_dot_ctx(void)
{
    return &g_nk_dot.ctx;
}

/*
 * Begin a new frame: feed RGFW input state into NK.
 * Call this once per frame, before issuing any nk_begin/nk_end calls.
 */
NK_API void
nk_dot_new_frame(DOT_Window *window)
{
    NkDot_State *s = &g_nk_dot;
    struct nk_context *ctx = &s->ctx;
    RGFW_window *rgfw = window->window;

    DOT_Extent2D e = dot_window_get_size(window);
    s->width  = (u32)e.x;
    s->height = (u32)e.y;

    nk_input_begin(ctx);

    /* Text input (collected via nk_dot_handle_event) */
    for (u32 i = 0; i < s->text_len; ++i)
        nk_input_unicode(ctx, s->text[i]);
    s->text_len = 0;

    /* Keyboard state */
    nk_input_key(ctx, NK_KEY_DEL,            RGFW_isKeyDown(RGFW_delete));
    nk_input_key(ctx, NK_KEY_ENTER,          RGFW_isKeyDown(RGFW_return));
    nk_input_key(ctx, NK_KEY_TAB,            RGFW_isKeyDown(RGFW_tab));
    nk_input_key(ctx, NK_KEY_BACKSPACE,      RGFW_isKeyDown(RGFW_backSpace));
    nk_input_key(ctx, NK_KEY_UP,             RGFW_isKeyDown(RGFW_up));
    nk_input_key(ctx, NK_KEY_DOWN,           RGFW_isKeyDown(RGFW_down));
    nk_input_key(ctx, NK_KEY_LEFT,           RGFW_isKeyDown(RGFW_left));
    nk_input_key(ctx, NK_KEY_RIGHT,          RGFW_isKeyDown(RGFW_right));
    nk_input_key(ctx, NK_KEY_SHIFT,          RGFW_isKeyDown(RGFW_shiftL) || RGFW_isKeyDown(RGFW_shiftR));
    nk_input_key(ctx, NK_KEY_SCROLL_UP,      0);
    nk_input_key(ctx, NK_KEY_SCROLL_DOWN,    0);

    /* Ctrl combos */
    {
        b32 ctrl = RGFW_isKeyDown(RGFW_controlL) || RGFW_isKeyDown(RGFW_controlR);
        nk_input_key(ctx, NK_KEY_COPY,            ctrl && RGFW_isKeyDown(RGFW_c));
        nk_input_key(ctx, NK_KEY_PASTE,           ctrl && RGFW_isKeyDown(RGFW_v));
        nk_input_key(ctx, NK_KEY_CUT,             ctrl && RGFW_isKeyDown(RGFW_x));
        nk_input_key(ctx, NK_KEY_TEXT_UNDO,       ctrl && RGFW_isKeyDown(RGFW_z));
        nk_input_key(ctx, NK_KEY_TEXT_REDO,       ctrl && RGFW_isKeyDown(RGFW_y));
        nk_input_key(ctx, NK_KEY_TEXT_SELECT_ALL,  ctrl && RGFW_isKeyDown(RGFW_a));
        nk_input_key(ctx, NK_KEY_TEXT_WORD_LEFT,   ctrl && RGFW_isKeyDown(RGFW_left));
        nk_input_key(ctx, NK_KEY_TEXT_WORD_RIGHT,  ctrl && RGFW_isKeyDown(RGFW_right));
        nk_input_key(ctx, NK_KEY_TEXT_LINE_START,  RGFW_isKeyDown(RGFW_home));
        nk_input_key(ctx, NK_KEY_TEXT_LINE_END,    RGFW_isKeyDown(RGFW_end));
    }

    /* Mouse position & buttons */
    {
        i32 mx, my;
        RGFW_window_getMouse(rgfw, &mx, &my);
        nk_input_motion(ctx, mx, my);

        if (RGFW_isMousePressed(RGFW_mouseLeft)) {
            f64 now = cast(f64)platform_get_time_ns() / (cast(f64)TO_USEC(1));
            f64 dt = now - s->last_button_click;
            if (dt > 20.0 && dt < 400.0) {
                s->is_double_click_down = nk_true;
                s->double_click_pos = nk_vec2((f32)mx, (f32)my);
            }
            s->last_button_click = now;
        }else{
            s->is_double_click_down = nk_false;
        }

        nk_input_button(ctx, NK_BUTTON_DOUBLE, (int)s->double_click_pos.x,
                         (int)s->double_click_pos.y, s->is_double_click_down);
        nk_input_button(ctx, NK_BUTTON_LEFT,   mx, my, RGFW_isMouseDown(RGFW_mouseLeft));
        nk_input_button(ctx, NK_BUTTON_MIDDLE, mx, my, RGFW_isMouseDown(RGFW_mouseMiddle));
        nk_input_button(ctx, NK_BUTTON_RIGHT,  mx, my, RGFW_isMouseDown(RGFW_mouseRight));
    }

    /* Scroll (collected via nk_dot_handle_event) */
    nk_input_scroll(ctx, s->scroll);
    s->scroll = nk_vec2(0, 0);

    nk_input_end(ctx);
}

/*
 * Feed an RGFW event into the NK bridge.
 * Call this from your event polling loop for events that need accumulation
 * (text input, scroll).
 */
NK_API void
nk_dot_handle_event(RGFW_event *event)
{
    NkDot_State *s = &g_nk_dot;
    if (!event) return;

    switch (event->type) {
    case RGFW_keyChar:
        if (s->text_len < NK_DOT_TEXT_MAX)
            s->text[s->text_len++] = event->keyChar.value;
        break;
    case RGFW_mouseScroll:
        s->scroll.x += event->scroll.x;
        s->scroll.y += event->scroll.y;
        break;
    default:
        break;
    }
}

/*
 * Render the Nuklear draw list.
 *
 * Converts NK draw commands into an OverlayDrawList and delegates all GPU
 * work through the renderer frontend (renderer_overlay_render).
 */
NK_API void
nk_dot_render(DOT_Renderer *renderer)
{
    NkDot_State *s = &g_nk_dot;
    struct nk_context *ctx = &s->ctx;
    u8 frame_idx = renderer->current_frame % renderer->backend->frame_overlap;

    /* Run nk_convert into CPU-side staging buffers */
    {
        static const struct nk_draw_vertex_layout_element vertex_layout[] = {
            { NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(OverlayVertex, position) },
            { NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(OverlayVertex, uv) },
            { NK_VERTEX_COLOR,    NK_FORMAT_R8G8B8A8, NK_OFFSETOF(OverlayVertex, col) },
            { NK_VERTEX_LAYOUT_END }
        };

        struct nk_convert_config config = {
            .vertex_layout        = vertex_layout,
            .vertex_size          = sizeof(OverlayVertex),
            .vertex_alignment     = NK_ALIGNOF(OverlayVertex),
            .tex_null             = s->tex_null,
            .circle_segment_count = 22,
            .curve_segment_count  = 22,
            .arc_segment_count    = 22,
            .global_alpha         = 1.0f,
            .shape_AA             = NK_ANTI_ALIASING_ON,
            .line_AA              = NK_ANTI_ALIASING_ON,
        };

        struct nk_buffer vbuf, ebuf;
        nk_buffer_init_fixed(&vbuf, s->vertex_buf, NK_DOT_MAX_VERTEX_BUFFER);
        nk_buffer_init_fixed(&ebuf, s->index_buf,  NK_DOT_MAX_ELEMENT_BUFFER);
        nk_convert(ctx, &s->cmds, &vbuf, &ebuf, &config);
    }

    /* Build backend-agnostic draw command array */
    OverlayDrawCmd draw_cmds[NK_DOT_MAX_DRAW_CMDS];
    u32 cmd_count = 0;
    const struct nk_draw_command *draw_cmd;
    nk_draw_foreach(draw_cmd, ctx, &s->cmds) {
        if (!draw_cmd->elem_count) continue;
        if (cmd_count >= NK_DOT_MAX_DRAW_CMDS) break;
        OverlayDrawCmd *dc = &draw_cmds[cmd_count++];
        dc->elem_count = draw_cmd->elem_count;
        dc->clip_x = draw_cmd->clip_rect.x;
        dc->clip_y = draw_cmd->clip_rect.y;
        dc->clip_w = draw_cmd->clip_rect.w;
        dc->clip_h = draw_cmd->clip_rect.h;
    }

    /* Delegate to renderer frontend */
    OverlayDrawList draw_list = {
        .vertices    = s->vertex_buf,
        .vertex_size = NK_DOT_MAX_VERTEX_BUFFER,
        .indices     = s->index_buf,
        .index_size  = NK_DOT_MAX_ELEMENT_BUFFER,
        .cmds        = draw_cmds,
        .cmd_count   = cmd_count,
        .width       = s->width,
        .height      = s->height,
    };
    renderer_overlay_render(renderer, frame_idx, &draw_list);

    nk_clear(ctx);
    nk_buffer_clear(&s->cmds);
}

/*
 * Shutdown: clean up NK context and tell the renderer to free overlay resources.
 */
NK_API void
nk_dot_shutdown(DOT_Renderer *renderer)
{
    NkDot_State *s = &g_nk_dot;

    renderer_overlay_shutdown(renderer);

    nk_font_atlas_clear(&s->font_atlas);
    nk_buffer_free(&s->cmds);
    nk_free(&s->ctx);

    MEMORY_ZERO_STRUCT(s);
}

#endif /* NK_DOT_WINDOW_H */
