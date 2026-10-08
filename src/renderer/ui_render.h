#ifndef UI_RENDER_H
#define UI_RENDER_H

#define UI_RENDER_MAX_VERTEX_BUFFER  (512 * 1024)
#define UI_RENDER_MAX_ELEMENT_BUFFER (128 * 1024)
#define UI_RENDER_SHADER_PATH        "src/renderer/shaders/"

typedef struct UI_RenderVertex {
    f32 position[2];
    f32 uv[2];
    u8  col[4];
} UI_RenderVertex;

typedef struct UI_Render {
    struct nk_font_atlas font_atlas;
    struct nk_buffer     cmds;
    struct nk_draw_null_texture tex_null;

    RN_BindlessTexture  font_texture;
    u32                 sampler_slot;
    RB_Pipeline         pipeline;

    // nk_convert output, copied into the frame heap afterwards.
    u8 vertex_buf[UI_RENDER_MAX_VERTEX_BUFFER];
    u8 index_buf[UI_RENDER_MAX_ELEMENT_BUFFER];
} UI_Render;

internal void ui_render_init(UI_Render *ui, RN_RenderCtx *renderer, struct nk_context *ctx);
internal void ui_render_draw(UI_Render *ui, RN_RenderCtx *renderer, struct nk_context *ctx, i32x2 ui_extent);
internal void ui_render_shutdown(UI_Render *ui, RN_RenderCtx *renderer);

#endif // !UI_RENDER_H
