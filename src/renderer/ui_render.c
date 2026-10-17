internal void
ui_render_init(UI_Render *ui, RN_RenderCtx *renderer, struct nk_context *ctx)
{
    MEMORY_ZERO_STRUCT(ui);
    nk_buffer_init_default(&ui->cmds);

    /* Bake font atlas (CPU-side) */
    const void *atlas_data;
    int atlas_w, atlas_h;
    nk_font_atlas_init_default(&ui->font_atlas);
    nk_font_atlas_begin(&ui->font_atlas);
    struct nk_font *font = nk_font_atlas_add_default(&ui->font_atlas, 14.0f, NULL);
    atlas_data = nk_font_atlas_bake(&ui->font_atlas, &atlas_w, &atlas_h, NK_FONT_ATLAS_RGBA32);

    RB_TextureDesc atlas_desc = {0};
    atlas_desc.dimension_kind = RB_TextureDimensionKind_2D;
    atlas_desc.format_kind    = RB_FormatKind_RGBA8_UNORM;
    atlas_desc.width          = cast(u16)atlas_w;
    atlas_desc.height         = cast(u16)atlas_h;
    atlas_desc.depth          = 1;
    atlas_desc.array_layers   = 1;
    atlas_desc.mip_levels     = 1;
    atlas_desc.debug_name     = str_comp("Nuklear font");

    ui->font_texture = rn_texture_create_bindless(renderer, &atlas_desc, cast(void*)atlas_data);

    RB_SamplerDesc sampler_desc = {0};
    sampler_desc.min_filter     = RB_SamplerFilterKind_Linear;
    sampler_desc.mag_filter     = RB_SamplerFilterKind_Linear;
    sampler_desc.address_mode_u = RB_SamplerAddressModeKind_ClampToEdge;
    sampler_desc.address_mode_v = RB_SamplerAddressModeKind_ClampToEdge;
    sampler_desc.address_mode_w = RB_SamplerAddressModeKind_ClampToEdge;
    ui->sampler_slot = rn_sampler_bindless_slot_create(renderer, &sampler_desc);

    RB_RasterDesc raster = {
        .raster_state.topology = RB_PrimitiveTopologyKind_TriangleList,
        .raster_state.cull_mode = RB_CullModeKind_None,
        .color_targets = { {
            .format = renderer->swapchain_format,
            .blend = {
                .blend_enabled = true,
                .src_color = RB_BlendFactorKind_SrcAlpha,
                .dst_color = RB_BlendFactorKind_OneMinusSrcAlpha,
                .blend_op  = RB_BlendOpKind_Add,
            },
        } },
        .debug_name = str_comp("Nuklear"),
    };

    ui->pipeline = rn_pipeline_create(renderer, str_comp(UI_RENDER_SHADER_PATH"nk_ui.vert"), str_comp(UI_RENDER_SHADER_PATH"nk_ui.frag"), &raster);

    nk_font_atlas_end(&ui->font_atlas, nk_handle_id(cast(int)ui->font_texture.bindless_slot), &ui->tex_null);
    nk_style_set_font(ctx, &font->handle);
}

internal void
ui_render_draw(UI_Render *ui, RN_RenderCtx *renderer, struct nk_context *ctx, i32x2 ui_extent)
{
    if(!renderer->current_frame_valid){
        nk_buffer_clear(&ui->cmds);
        return;
    }

    struct nk_buffer vbuf, ebuf;
    {
        static const struct nk_draw_vertex_layout_element vertex_layout[] = {
            { NK_VERTEX_POSITION, NK_FORMAT_FLOAT,    NK_OFFSETOF(UI_RenderVertex, position) },
            { NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT,    NK_OFFSETOF(UI_RenderVertex, uv) },
            { NK_VERTEX_COLOR,    NK_FORMAT_R8G8B8A8, NK_OFFSETOF(UI_RenderVertex, col) },
            { NK_VERTEX_LAYOUT_END }
        };

        struct nk_convert_config config = {
            .vertex_layout        = vertex_layout,
            .vertex_size          = sizeof(UI_RenderVertex),
            .vertex_alignment     = NK_ALIGNOF(UI_RenderVertex),
            .tex_null             = ui->tex_null,
            .circle_segment_count = 22,
            .curve_segment_count  = 22,
            .arc_segment_count    = 22,
            .global_alpha         = 1.0f,
            .shape_AA             = NK_ANTI_ALIASING_ON,
            .line_AA              = NK_ANTI_ALIASING_ON,
        };

        nk_buffer_init_fixed(&vbuf, ui->vertex_buf, UI_RENDER_MAX_VERTEX_BUFFER);
        nk_buffer_init_fixed(&ebuf, ui->index_buf,  UI_RENDER_MAX_ELEMENT_BUFFER);
        nk_convert(ctx, &ui->cmds, &vbuf, &ebuf, &config);
    }
    if(vbuf.allocated == 0 || ebuf.allocated == 0){
        nk_buffer_clear(&ui->cmds);
        return;
    }
    RB_GpuRange vertices = rn_frame_push_(renderer, ui->vertex_buf, cast(i64)vbuf.allocated);
    RB_GpuRange indices  = rn_frame_push_(renderer, ui->index_buf,  cast(i64)ebuf.allocated);

    RB_CommandBuffer cmd = renderer->current_cmd;
    i32x2 extent  = renderer->current_extent;
    u32 ui_width  = cast(u32)DOT_MAX(ui_extent.x, 1);
    u32 ui_height = cast(u32)DOT_MAX(ui_extent.y, 1);
    // Nuklear works in window units; the framebuffer may be larger on scaled outputs.
    f32 fb_scale_x = cast(f32)extent.x / cast(f32)ui_width;
    f32 fb_scale_y = cast(f32)extent.y / cast(f32)ui_height;

    RB_RenderingDesc desc = {0};
    desc.render_extent = extent;
    desc.color_attachments[0].view     = renderer->current_view;
    desc.color_attachments[0].load_op  = RB_LoadOpKind_Load;
    desc.color_attachments[0].store_op = RB_StoreOpKind_Store;
    rb_barrier(cmd, RB_StageBit_ColorOutput, RB_AccessBit_ColorWrite,
        RB_StageBit_ColorOutput, RB_AccessBit_ColorRead | RB_AccessBit_ColorWrite);
    rb_render_pass_begin(cmd, desc, RB_RenderingBit_None);
    rb_bind_pso(cmd, ui->pipeline);

    UI_RenderRoot root = {0};
    root.scale        = (f32x2){ .x = 2.0f / cast(f32)ui_width, .y = 2.0f / cast(f32)ui_height };
    root.translate    = (f32x2){ .x = -1.0f, .y = -1.0f };
    root.vertex_addr  = vertices.gpu;
    root.sampler_slot = ui->sampler_slot;

    u32 first_index = 0;
    const struct nk_draw_command *draw_cmd;
    nk_draw_foreach(draw_cmd, ctx, &ui->cmds){
        if(!draw_cmd->elem_count){ continue; }

        f32 x0 = DOT_CLAMP(draw_cmd->clip_rect.x * fb_scale_x, 0.0f, cast(f32)extent.x);
        f32 y0 = DOT_CLAMP(draw_cmd->clip_rect.y * fb_scale_y, 0.0f, cast(f32)extent.y);
        f32 x1 = DOT_CLAMP((draw_cmd->clip_rect.x + draw_cmd->clip_rect.w) * fb_scale_x, 0.0f, cast(f32)extent.x);
        f32 y1 = DOT_CLAMP((draw_cmd->clip_rect.y + draw_cmd->clip_rect.h) * fb_scale_y, 0.0f, cast(f32)extent.y);
        if(x1 > x0 && y1 > y0){
            rb_set_scissor(cmd, (RB_Extent2D16){ .x = cast(i16)x0, .y = cast(i16)y0, .w = cast(u16)(x1 - x0), .h = cast(u16)(y1 - y0) });
            root.texture_slot = cast(u32)draw_cmd->texture.id;
            RB_GpuRange root_range = rn_frame_push(renderer, root);
            rb_draw_indexed(cmd, root_range.gpu, indices, RB_IndexKind_U16, draw_cmd->elem_count, 1, first_index, 0, 0);
        }
        first_index += draw_cmd->elem_count;
    }

    rb_render_pass_end(cmd);

    nk_buffer_clear(&ui->cmds);
}

internal void
ui_render_shutdown(UI_Render *ui, RN_RenderCtx *renderer)
{
    rn_pipeline_destroy(renderer, ui->pipeline);
    rn_texture_destroy(renderer, ui->font_texture.handle);

    nk_font_atlas_clear(&ui->font_atlas);
    nk_buffer_free(&ui->cmds);
    MEMORY_ZERO_STRUCT(ui);
}
