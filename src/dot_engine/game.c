global DOT_Game *g_game = NULL;

#ifdef DOT_HOT_RELOAD
void dot_game_init(DOT_Game *game, RN_RenderCtx *renderer,
    u8 permanent_memory[], usize permanent_memory_size,
    u8 transient_memory[], usize transient_memory_size) {
    g_game_api.init(game);
}

void dot_game_shutdown(DOT_Game *game) {
    g_game_api.shutdown(game);
}

void dot_game_run(DOT_Game *game) {
    g_game_api.run(game);
}

#else
// NOTE: All this shouldn't we here but I am using this as a testbed for the render api for now
b32 dot_game_init(DOT_Game *game, RN_RenderCtx *renderer,
    u8 permanent_memory[], usize permanent_memory_size,
    u8 transient_memory[], usize transient_memory_size)
{
    g_game = game;
    g_game->renderer = renderer;
    g_game->permanent_arena = ARENA_CREATE(.name = "engine permanent arena", .buffer = permanent_memory, .reserve_size = permanent_memory_size);
    g_game->transient_arena = ARENA_CREATE(.name = "engine transient arena", .buffer = transient_memory, .reserve_size = transient_memory_size);

    // Buncha tests
    g_game->test_shader_module = rn_shader_module_load_from_path(game->renderer, string8_lit(DOT_GAME_SHADER_PATH"compute.glsl"));
    rn_create_postprocess_module(g_game->test_shader_module->shader_module_handle);

    // String8 model_path = string8_lit(DOT_GAME_ASSET_PATH"glTF-Sample-Models/2.0/DamagedHelmet/glTF-Embedded/DamagedHelmet.gltf");
    // String8 model_path = string8_lit(DOT_GAME_ASSET_PATH"glTF-Sample-Models/2.0/DamagedHelmet/glTF/DamagedHelmet.gltf");
    String8 model_path = string8_lit(DOT_GAME_ASSET_PATH"glTF-Sample-Models/2.0/Cube/glTF/Cube.gltf");
    // String8 model_path = string8_lit(DOT_GAME_ASSET_PATH"glTF-Sample-Models/2.0/2CylinderEngine/glTF/2CylinderEngine.gltf");
    DOT_Model model = dot_model_load_from_path(g_game->renderer, model_path);

    String8 shader_stages[2] = {
        [RN_ShaderStageKind_Vertex] =
        #include "../src/game/shaders/model.vert"
        , [RN_ShaderStageKind_Fragment] =
        #include "../src/game/shaders/model.frag"
    };
    RN_PipelineDesc pipeline = {
        .vertex_input = {
            // .vertex_attribute_count = DOT_ARRAY_COUNT(vertex_attributes),
            // .vertex_attributes = vertex_attributes,
            .vertex_attributes = ARRAY_LIT(RN_VertexAttribute,
                { .location = 0, .binding = 0, .offset = 0, .vertex_component_kind = RN_VertexCommponentKind_F32x3 },
                { .location = 1, .binding = 1, .offset = 0, .vertex_component_kind = RN_VertexCommponentKind_F32x4 },
                { .location = 2, .binding = 2, .offset = 0, .vertex_component_kind = RN_VertexCommponentKind_F32x3 },
                { .location = 3, .binding = 3, .offset = 0, .vertex_component_kind = RN_VertexCommponentKind_F32x2 },
            ),
            .vertex_streams = ARRAY_LIT(RN_VertexStream,
                { .binding = 0, .stride = 12, .input_rate = RN_VertexInputRateKind_PerVertex},
                { .binding = 1, .stride = 16, .input_rate = RN_VertexInputRateKind_PerVertex},
                { .binding = 2, .stride = 12, .input_rate = RN_VertexInputRateKind_PerVertex},
                { .binding = 3, .stride = 8, .input_rate = RN_VertexInputRateKind_PerVertex},
            ),
        },
        .depth_stencil_state = {
            .depth_write_enable = true,
            .depth_enable = true,
            .depth_comparison = RN_CompareOp_LessOrEqual,
        },
        .shader_state = {
            .format = RN_ShaderFormat_Source,
            .shader_stages = ARRAY_LIT(RN_ShaderStage,
                {.stage = RN_ShaderStageKind_Vertex,   .code = shader_stages[RN_ShaderStageKind_Vertex]},
                {.stage = RN_ShaderStageKind_Fragment, .code = shader_stages[RN_ShaderStageKind_Fragment]}
            ),
        },
        .render_pass = rn_swapchain_output(),
    };
    RN_ShaderResourceLayoutDesc desc = rn_shader_resource_layout_begin();
    rn_shader_resource_layout_set_bindings(&desc,
        { RN_ShaderResourceKind_UniformBufferDynamic, 0, 1, string8_lit("LocalConstants"), 0 },
        { RN_ShaderResourceKind_UniformBufferDynamic, 1, 1, string8_lit("MaterialConstants"), 0 },
        { RN_ShaderResourceKind_SamplerXTexture, 2, 1, string8_lit("diffuseTexture"), 0 },
        { RN_ShaderResourceKind_SamplerXTexture, 3, 1, string8_lit("roughnessMetalnessTexture"), 0 },
        { RN_ShaderResourceKind_SamplerXTexture, 4, 1, string8_lit("roughnessMetalnessTexture"), 0 },
        { RN_ShaderResourceKind_SamplerXTexture, 5, 1, string8_lit("emissiveTexture"), 0 },
        { RN_ShaderResourceKind_SamplerXTexture, 6, 1, string8_lit("occlusionTexture"), 0 },
    );
    RN_ShaderResourceLayoutHandle h = rn_shader_resource_layout_create(renderer, &desc);
    pipeline.descriptor_set_layouts[0] = h;
    ++pipeline.descriptor_set_layout_count;

    RN_PipelineHandle cube_pipeline = rn_pipeline_create(renderer, &pipeline);
    (void)cube_pipeline;


    // RN_PipelineDesc pipeline2 = {0};
    // rn_pipeline_set_depth_stencil_state(&pipeline2,
    //     .depth_write_enable = true,
    //     .depth_enable = true,
    //     .depth_comparison = RN_CompareOp_LessOrEqual
    // );
    // rn_pipeline_set_vertex_attributes(&pipeline2,
    //     { .location = 0, .binding = 0, .offset = 0, .vertex_component_kind = RN_VertexCommponentKind_F32x3 },
    //     { .location = 1, .binding = 1, .offset = 0, .vertex_component_kind = RN_VertexCommponentKind_F32x4 },
    //     { .location = 2, .binding = 2, .offset = 0, .vertex_component_kind = RN_VertexCommponentKind_F32x3 },
    //     { .location = 3, .binding = 3, .offset = 0, .vertex_component_kind = RN_VertexCommponentKind_F32x2 },
    // );
    // rn_pipeline_set_vertex_streams(&pipeline2,
    //     { .binding = 0, .stride = 12, .input_rate = RN_VertexInputRateKind_PerVertex},
    //     { .binding = 1, .stride = 16, .input_rate = RN_VertexInputRateKind_PerVertex},
    //     { .binding = 2, .stride = 12, .input_rate = RN_VertexInputRateKind_PerVertex},
    //     { .binding = 3, .stride = 8,  .input_rate = RN_VertexInputRateKind_PerVertex},
    // );
    // String8 shader_stages2[2] = {
    //     [RN_ShaderStageKind_Vertex] =
    //     #include "../src/game/shaders/model.vert"
    //     , [RN_ShaderStageKind_Fragment] =
    //     #include "../src/game/shaders/model.frag"
    // };
    //
    // rn_pipeline_set_shader_state(&pipeline2, RN_ShaderFormat_Source, string8_lit("Cube"),
    //     {RN_ShaderStageKind_Vertex,  shader_stages2[RN_ShaderStageKind_Vertex]},
    //     {RN_ShaderStageKind_Fragment, shader_stages2[RN_ShaderStageKind_Fragment]},
    // );
    // rn_pipeline_set_render_pass_ouput(&pipeline2, rn_swapchain_output());

    // string8_append_string8(Arena *arena, String8 a, String8 b)
    // DOT_DEBUG_NAME_SET(pipeline.shader_state.name, string8_lit("cube")),


    (void) pipeline;
    rn_pipeline_create(renderer, &pipeline);
    // const char *t = "src/game/" "shaders/" "model.frag";
    // typedef struct PipelineStages{
    // }PipelineStages;

    // const char *code[2] = {

    // pipeline_creation.shaders.set_name( "Cube" ).add_stage( vs_code, ( uint32_t )strlen( vs_code ), VK_SHADER_STAGE_VERTEX_BIT ).add_stage( fs_code, ( uint32_t )strlen( fs_code ), VK_SHADER_STAGE_FRAGMENT_BIT );
    (void)shader_stages;


    // RN_PipelineDesc pipeline2 = {
    //     .vertex_input = {
    //         SLICE_FIELDS(RN_VertexAttribute, vertex_attributes, vertex_attribute_count,
    //                 { .location = 0, .binding = 0, .offset = 0, .vertex_component_kind = RN_VertexCommponentKind_F32x3 },
    //                 { .location = 1, .binding = 1, .offset = 0, .vertex_component_kind = RN_VertexCommponentKind_F32x4 },
    //                 { .location = 2, .binding = 2, .offset = 0, .vertex_component_kind = RN_VertexCommponentKind_F32x3 },
    //                 { .location = 3, .binding = 3, .offset = 0, .vertex_component_kind = RN_VertexCommponentKind_F32x2 },
    //         ),
    //         SLICE_FIELDS(RN_VertexStream, vertex_streams, vertex_stream_count,
    //             { 0, 12, RN_VertexInputRateKind_PerVertex},
    //             { 1, 16, RN_VertexInputRateKind_PerVertex},
    //             { 2, 12, RN_VertexInputRateKind_PerVertex},
    //             { 3, 8, RN_VertexInputRateKind_PerVertex},
    //         ),
    //     },
    // };
    // RN_PipelineDesc pipeline = {
    //     .vertex_input = {
    //         .vertex_attribute_count = 4,
    //         .vertex_stream_count = 4,
    //         .vertex_attributes = {
    //             { .location = 0, .binding = 0, .offset = 0, .vertex_component_kind = RN_VertexCommponentKind_F32x3 }, // pos
    //             { .location = 1, .binding = 1, .offset = 0, .vertex_component_kind = RN_VertexCommponentKind_F32x4 }, // tangent
    //             { .location = 2, .binding = 2, .offset = 0, .vertex_component_kind = RN_VertexCommponentKind_F32x3 }, // normal
    //             { .location = 3, .binding = 3, .offset = 0, .vertex_component_kind = RN_VertexCommponentKind_F32x2 }, // texcoord
    //         },
    //         .vertex_streams = {
    //             { 0, 12, RN_VertexInputRateKind_PerVertex},
    //             { 1, 16, RN_VertexInputRateKind_PerVertex},
    //             { 2, 12, RN_VertexInputRateKind_PerVertex},
    //             { 3, 8, RN_VertexInputRateKind_PerVertex},
    //         },
    //     },
    // };
    (void)model;
    return true;
}

void dot_game_run(DOT_Game *game)
{
    RN_RenderCtx *renderer = game->renderer;
    // f64 flash = fabs(sin(renderer->current_frame / 60.f));
    u32 current_frame = 0;
    f32 flash = cast(f32)((sin(cast(f32)current_frame / 1200.f)+1.f) / 2.0f);
    rn_clear_background(renderer, v3(0,0, cast(f32)flash));
}

void dot_game_shutdown(DOT_Game* game)
{
    (void)game;
}

#endif
