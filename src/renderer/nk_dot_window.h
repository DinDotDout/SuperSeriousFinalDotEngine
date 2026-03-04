#include "base/arena.h"
#include "base/dot.h"
#include "base/thread_ctx.h"
#include "renderer/nk_shaders.c"

#define NK_RGFW_TEXT_MAX 1024

#include "dot_engine/window.h"

#define NK_SHADER_VERSION "#version 450 core\n"
#define NK_SHADER_BINDLESS "#extension GL_ARB_bindless_texture : require\n"
#define NK_SHADER_64BIT "#extension GL_ARB_gpu_shader_int64 : require\n"

#define MAX_VERTEX_BUFFER 512 * 1024
#define MAX_ELEMENT_BUFFER 128 * 1024
typedef struct NkRgfw_Renderer{
    RendererBackendKind bck_kind;
    union RendererBackendK{
        RendererBackendVk vk;
    }*renderer;

    struct nk_buffer cmds;
    struct nk_draw_null_texture tex_null;
    u32 max_vertex_buffer;
    u32 max_element_buffer;
}NkRgfw_Renderer;

typedef struct NkRgfw_Vertex{
    f32 position[2];
    f32 uv[2];
    nk_byte col[4];
}NkRgfw_Vertex;

typedef struct Nk_RGFW{
    DOT_Window *win;
    u32 width, height;
    u32 display_width, display_height;
    struct NkRgfw_Renderer vk_device;

    struct nk_context ctx;
    struct nk_font_atlas font_atlas;
    struct nk_vec2 fb_scale;
    u32 text[NK_RGFW_TEXT_MAX];
    u32 text_len;
    struct nk_vec2 scroll;
    f64 last_button_click;
    u32 is_double_click_down; // NOTE: use b8
    struct nk_vec2 double_click_pos;
}Nk_RGFW;
global Nk_RGFW rgfw_ctx;

// RGFW_writeClipboard
// RGFW_readClipboardPtr

NK_INTERN void
nk_rgfw_clipboard_paste(nk_handle usr, struct nk_text_edit *edit)
{
    UNUSED(usr);
    usize len = 0;
    const char *text = RGFW_readClipboard(&len);
    if (text)
        nk_textedit_paste(edit, text, len);
}

NK_INTERN void
nk_rgfw_clipboard_copy(nk_handle usr, const char *text, int len)
{
    UNUSED(usr);
    if (!len)
        return;
    RGFW_writeClipboard(text, len);
}

NK_API struct nk_context*
nk_dot_window_init(DOT_Window *win, u32 max_vertex_buffer, u32 max_element_buffer)
{
    (void) max_vertex_buffer;
    (void) max_element_buffer;
    MEMORY_ZERO_STRUCT(&rgfw_ctx);
    rgfw_ctx.win = win;
    nk_init_default(&rgfw_ctx.ctx, NULL);
    rgfw_ctx.ctx.clip.copy = nk_rgfw_clipboard_copy;
    rgfw_ctx.ctx.clip.paste = nk_rgfw_clipboard_paste;
    rgfw_ctx.ctx.clip.userdata = nk_handle_ptr(0);
    rgfw_ctx.last_button_click = 0;
    rgfw_ctx.is_double_click_down = nk_false;
    rgfw_ctx.double_click_pos = nk_vec2(0, 0);
    return &rgfw_ctx.ctx;
}
