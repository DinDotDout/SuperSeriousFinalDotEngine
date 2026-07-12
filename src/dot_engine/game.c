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
    // g_game->test_shader_module = rn_shader_module_load_from_path(game->renderer, string8_lit(DOT_GAME_SHADER_PATH"compute.glsl"));
    // rn_create_postprocess_module(g_game->test_shader_module->shader_stage_handle);

    // String8 model_path = string8_lit(DOT_GAME_ASSET_PATH"glTF-Sample-Models/2.0/DamagedHelmet/glTF-Embedded/DamagedHelmet.gltf");
    // String8 model_path = string8_lit(DOT_GAME_ASSET_PATH"glTF-Sample-Models/2.0/DamagedHelmet/glTF/DamagedHelmet.gltf");
    String8 model_path = string8_lit(DOT_GAME_ASSET_PATH"glTF-Sample-Models/2.0/Cube/glTF/Cube.gltf");
    // String8 model_path = string8_lit(DOT_GAME_ASSET_PATH"glTF-Sample-Models/2.0/2CylinderEngine/glTF/2CylinderEngine.gltf");
    DOT_Scene model = dot_scene_load_from_path(g_game->renderer, model_path);

    RN_ShaderResourceLayoutDesc resource_layout_desc = {0};
    rn_shader_resource_layout_set_bindings(&resource_layout_desc,
        { RN_ShaderResourceKind_UniformBufferDynamic, 0, 1, string8_lit("LocalConstants"), 0 },
        { RN_ShaderResourceKind_UniformBufferDynamic, 1, 1, string8_lit("MaterialConstants"), 0 },
        { RN_ShaderResourceKind_SamplerXTexture, 2, 1, string8_lit("diffuseTexture"), 0 },
        { RN_ShaderResourceKind_SamplerXTexture, 3, 1, string8_lit("roughnessMetalnessTexture"), 0 },
        { RN_ShaderResourceKind_SamplerXTexture, 4, 1, string8_lit("roughnessMetalnessTexture"), 0 },
        { RN_ShaderResourceKind_SamplerXTexture, 5, 1, string8_lit("emissiveTexture"), 0 },
        { RN_ShaderResourceKind_SamplerXTexture, 6, 1, string8_lit("occlusionTexture"), 0 },
    );
    g_shader_resource_layout_h = rn_shader_resource_layout_create(renderer, &resource_layout_desc);

    RN_PipelineDesc pipeline_desc = {0};
    rn_pipeline_set_vertex_attributes(&pipeline_desc,
        { .location = 0, .binding = 0, .offset = 0, .vertex_component_kind = RN_FormatKind_F32x3 }, // position
        { .location = 1, .binding = 1, .offset = 0, .vertex_component_kind = RN_FormatKind_F32x4 }, // tangent
        { .location = 2, .binding = 2, .offset = 0, .vertex_component_kind = RN_FormatKind_F32x3 }, // normal
        { .location = 3, .binding = 3, .offset = 0, .vertex_component_kind = RN_FormatKind_F32x2 }  // texcoord
    );
    rn_pipeline_set_vertex_streams(&pipeline_desc,
        { .binding = 0, .stride = 12, .input_rate = RN_VertexInputRateKind_PerVertex}, // position
        { .binding = 1, .stride = 16, .input_rate = RN_VertexInputRateKind_PerVertex}, // tangent
        { .binding = 2, .stride = 12, .input_rate = RN_VertexInputRateKind_PerVertex}, // normal
        { .binding = 3, .stride = 8, .input_rate = RN_VertexInputRateKind_PerVertex},  // texcoord
    );
    rn_pipeline_set_depth_stencil_state(&pipeline_desc,
        .depth_write_enable = true,
        .depth_enable = true,
        .depth_comparison = RN_CompareOpKind_LessOrEqual
    );
    rn_pipeline_set_shader_state(&pipeline_desc,
        { .path = string8_lit(DOT_GAME_SHADER_PATH"model.vert") },
        { .path = string8_lit(DOT_GAME_SHADER_PATH"model.frag") },
    );
    DOT_DebugNameSet(pipeline_desc.shader_state.debug_name, string8_lit("Cube"));

    rn_pipeline_set_render_pass_output(&pipeline_desc, rn_swapchain_output());
    rn_pipeline_push_shader_resource_layout(&pipeline_desc, g_shader_resource_layout_h);

    typedef struct UniformData{
        mat4 model_from_view;
        mat4 view_from_projection;
        vec4 eye;
        vec4 light;
    }UniformData;

    RN_BufferDesc bd = {0};
    bd.resource_usage       = RN_ResourceUsageKind_Dynamic;
    bd.buffer_usage_flags   = RN_BufferUsageBit_Uniform;
    bd.size = sizeof(UniformData);
    DOT_DebugNameSet(bd.debug_name, string8_lit("cube constant buffer"));

    g_cube_ubo_h = rn_buffer_create_h(renderer, &bd, NULL);
    (void)g_cube_ubo_h;

    RN_PipelineHandle cube_pipeline = rn_pipeline_create(renderer, &pipeline_desc);
    (void)cube_pipeline;

    // TODO: put alloc somewhere it makes sense
    // RN_ShaderResourceDesc *descriptor_sets = PUSH_ARRAY(game->permanent_arena, RN_ShaderResourceDesc, model.sub_mesh_draw_count);

    // for EACH_INDEX(i, model.sub_mesh_draw_count){
        // DOT_Submesh *submesh = &model.sub_mesh_draw[i];
        // submesh->buffers
        // RN_ShaderResourceDesc *descriptor_set = &descriptor_sets[i];
        // if(
    // }
    //

    // RN_PipelineDesc pipeline2 = {0};
    // rn_pipeline_set_depth_stencil_state(&pipeline2,
    //     .depth_write_enable = true,
    //     .depth_enable = true,
    //     .depth_comparison = RN_CompareOpKind_LessOrEqual
    // );
    // rn_pipeline_set_vertex_attributes(&pipeline2,
    //     { .location = 0, .binding = 0, .offset = 0, .vertex_component_kind = RN_FormatKind_F32x3 },
    //     { .location = 1, .binding = 1, .offset = 0, .vertex_component_kind = RN_FormatKind_F32x4 },
    //     { .location = 2, .binding = 2, .offset = 0, .vertex_component_kind = RN_FormatKind_F32x3 },
    //     { .location = 3, .binding = 3, .offset = 0, .vertex_component_kind = RN_FormatKind_F32x2 },
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
    // DOT_DebugNameSet(pipeline.shader_state.name, string8_lit("cube")),


    // (void) pipeline;
    // rn_pipeline_create(renderer, &pipeline);
    // const char *t = "src/game/" "shaders/" "model.frag";
    // typedef struct PipelineStages{
    // }PipelineStages;

    // const char *code[2] = {

    // pipeline_creation.shaders.set_name( "Cube" ).add_stage( vs_code, ( uint32_t )strlen( vs_code ), VK_SHADER_STAGE_VERTEX_BIT ).add_stage( fs_code, ( uint32_t )strlen( fs_code ), VK_SHADER_STAGE_FRAGMENT_BIT );
    // (void)shader_stages;


    // RN_PipelineDesc pipeline2 = {
    //     .vertex_input = {
    //         SLICE_FIELDS(RN_VertexAttribute, vertex_attributes, vertex_attribute_count,
    //                 { .location = 0, .binding = 0, .offset = 0, .vertex_component_kind = RN_FormatKind_F32x3 },
    //                 { .location = 1, .binding = 1, .offset = 0, .vertex_component_kind = RN_FormatKind_F32x4 },
    //                 { .location = 2, .binding = 2, .offset = 0, .vertex_component_kind = RN_FormatKind_F32x3 },
    //                 { .location = 3, .binding = 3, .offset = 0, .vertex_component_kind = RN_FormatKind_F32x2 },
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
    //             { .location = 0, .binding = 0, .offset = 0, .vertex_component_kind = RN_FormatKind_F32x3 }, // pos
    //             { .location = 1, .binding = 1, .offset = 0, .vertex_component_kind = RN_FormatKind_F32x4 }, // tangent
    //             { .location = 2, .binding = 2, .offset = 0, .vertex_component_kind = RN_FormatKind_F32x3 }, // normal
    //             { .location = 3, .binding = 3, .offset = 0, .vertex_component_kind = RN_FormatKind_F32x2 }, // texcoord
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
