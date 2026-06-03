global DOT_Game *g_game = NULL;

#ifdef DOT_HOT_RELOAD
void dot_game_init(DOT_Game *game, DOT_Renderer *renderer,
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
b32 dot_game_init(DOT_Game *game, DOT_Renderer *renderer,
    u8 permanent_memory[], usize permanent_memory_size,
    u8 transient_memory[], usize transient_memory_size)
{
    g_game = game;
    g_game->renderer = renderer;
    g_game->permanent_arena = ARENA_CREATE(.name = "engine permanent arena", .buffer = permanent_memory, .reserve_size = permanent_memory_size);
    g_game->transient_arena = ARENA_CREATE(.name = "engine transient arena", .buffer = transient_memory, .reserve_size = transient_memory_size);

    g_game->test_shader_module = renderer_shader_module_load_from_path(game->renderer, String8Lit(DOT_GAME_SHADER_PATH"compute.glsl"));
    renderer_create_postprocess_module(g_game->test_shader_module->shader_module_handle);

    // String8 model_path = String8Lit(DOT_GAME_ASSET_PATH"glTF-Sample-Models/2.0/DamagedHelmet/glTF-Embedded/DamagedHelmet.gltf");
    String8 model_path = String8Lit(DOT_GAME_ASSET_PATH"glTF-Sample-Models/2.0/DamagedHelmet/glTF/DamagedHelmet.gltf");
    // String8 model_path = String8Lit(DOT_GAME_ASSET_PATH"glTF-Sample-Models/2.0/2CylinderEngine/glTF/2CylinderEngine.gltf");
    DOT_Model model = dot_model_load_from_path(g_game->renderer, model_path);

    RenderTypes_Pipeline pipeline = {
        .vertex_input = {
            .vertex_attributes = ARRAY_INIT(RenderTypes_VertexAttribute,
                { .location = 0, .binding = 0, .offset = 0, .vertex_component_kind = RenderTypes_VertexCommponentKind_F32x3 },
                { .location = 1, .binding = 1, .offset = 0, .vertex_component_kind = RenderTypes_VertexCommponentKind_F32x4 },
                { .location = 2, .binding = 2, .offset = 0, .vertex_component_kind = RenderTypes_VertexCommponentKind_F32x3 },
                { .location = 3, .binding = 3, .offset = 0, .vertex_component_kind = RenderTypes_VertexCommponentKind_F32x2 },
            ),
            .vertex_streams = ARRAY_INIT(RenderTypes_VertexStream,
                { .binding = 0, .stride = 12, .input_rate = RenderTypes_VertexInputRateKind_PerVertex},
                { .binding = 1, .stride = 16, .input_rate = RenderTypes_VertexInputRateKind_PerVertex},
                { .binding = 2, .stride = 12, .input_rate = RenderTypes_VertexInputRateKind_PerVertex},
                { .binding = 3, .stride = 8, .input_rate = RenderTypes_VertexInputRateKind_PerVertex},
            ),
        },
    };
    (void) pipeline;

    // RenderTypes_Pipeline pipeline2 = {
    //     .vertex_input = {
    //         SLICE_FIELDS(RenderTypes_VertexAttribute, vertex_attributes, vertex_attribute_count,
    //                 { .location = 0, .binding = 0, .offset = 0, .vertex_component_kind = RenderTypes_VertexCommponentKind_F32x3 },
    //                 { .location = 1, .binding = 1, .offset = 0, .vertex_component_kind = RenderTypes_VertexCommponentKind_F32x4 },
    //                 { .location = 2, .binding = 2, .offset = 0, .vertex_component_kind = RenderTypes_VertexCommponentKind_F32x3 },
    //                 { .location = 3, .binding = 3, .offset = 0, .vertex_component_kind = RenderTypes_VertexCommponentKind_F32x2 },
    //         ),
    //         SLICE_FIELDS(RenderTypes_VertexStream, vertex_streams, vertex_stream_count,
    //             { 0, 12, RenderTypes_VertexInputRateKind_PerVertex},
    //             { 1, 16, RenderTypes_VertexInputRateKind_PerVertex},
    //             { 2, 12, RenderTypes_VertexInputRateKind_PerVertex},
    //             { 3, 8, RenderTypes_VertexInputRateKind_PerVertex},
    //         ),
    //     },
    // };
    // RenderTypes_Pipeline pipeline = {
    //     .vertex_input = {
    //         .vertex_attribute_count = 4,
    //         .vertex_stream_count = 4,
    //         .vertex_attributes = {
    //             { .location = 0, .binding = 0, .offset = 0, .vertex_component_kind = RenderTypes_VertexCommponentKind_F32x3 }, // pos
    //             { .location = 1, .binding = 1, .offset = 0, .vertex_component_kind = RenderTypes_VertexCommponentKind_F32x4 }, // tangent
    //             { .location = 2, .binding = 2, .offset = 0, .vertex_component_kind = RenderTypes_VertexCommponentKind_F32x3 }, // normal
    //             { .location = 3, .binding = 3, .offset = 0, .vertex_component_kind = RenderTypes_VertexCommponentKind_F32x2 }, // texcoord
    //         },
    //         .vertex_streams = {
    //             { 0, 12, RenderTypes_VertexInputRateKind_PerVertex},
    //             { 1, 16, RenderTypes_VertexInputRateKind_PerVertex},
    //             { 2, 12, RenderTypes_VertexInputRateKind_PerVertex},
    //             { 3, 8, RenderTypes_VertexInputRateKind_PerVertex},
    //         },
    //     },
    // };
    (void)model;
    return true;
}

void dot_game_run(DOT_Game *game)
{
    DOT_Renderer *renderer = game->renderer;
    // f64 flash = fabs(sin(renderer->current_frame / 60.f));
    u32 current_frame = 0;
    f32 flash = cast(f32)((sin(cast(f32)current_frame / 1200.f)+1.f) / 2.0f);
    renderer_clear_background(renderer, v3(0,0, cast(f32)flash));
}

void dot_game_shutdown(DOT_Game* game)
{
    (void)game;
}

#endif
