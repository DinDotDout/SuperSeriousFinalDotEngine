global DOT_Game *g_game = NULL;

#ifdef DOT_HOT_RELOAD
void dot_game_init(DOT_Game *game, RN_RenderCtx *renderer, DOT_Window *window,
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

#include "game/shaders/triangle_shared.h"

RB_Pipeline g_triangle_pipeline = {0};
RB_Pipeline g_cube_pipeline = {0};

RB_GpuAddress     g_triangleMesh = {0};
RB_GpuAddress     g_cubeMesh = {0};

RB_Pipeline g_model_pipeline = {0};
DOT_Model         g_model = {0};

Reflect(DOT_Camera, {
    f32x3 position;
    f32   yaw;
    f32   pitch;
    f32   fov_y;
});

DOT_Camera g_camera = {0};

internal f32x3
dot_camera_forward(DOT_Camera *camera)
{
    return v3(
        sinf(camera->yaw) * cosf(camera->pitch),
        sinf(camera->pitch),
        -cosf(camera->yaw) * cosf(camera->pitch));
}

internal void
dot_camera_update(DOT_Camera *camera, DOT_Window *dot_window, f32 dt)
{
    local_persist const f32 look_speed = 0.004f;
    local_persist const f32 move_speed = 35.0f;
    local_persist const f32 pitch_limit = DOT_PI*0.5f - 0.01f;

    RGFW_window *window = dot_window->window;
    if(!RGFW_window_isCaptured(window)){ return; } // UI mode (F1)
    f32 mouse_dx = dot_window->mouse_delta.x, mouse_dy = dot_window->mouse_delta.y;
    camera->yaw   += mouse_dx * look_speed;
    camera->pitch  = DOT_CLAMP(camera->pitch - mouse_dy*look_speed, -pitch_limit, pitch_limit);

    f32x3 forward = dot_camera_forward(camera);
    f32x3 right   = f32x3_normalize(f32x3_cross(forward, f32x3_up));

    f32x3 move = f32x3_zero;
    if(RGFW_window_isKeyDown(window, RGFW_keyW)){ move = f32x3_add(move, forward); }
    if(RGFW_window_isKeyDown(window, RGFW_keyS)){ move = f32x3_sub(move, forward); }
    if(RGFW_window_isKeyDown(window, RGFW_keyD)){ move = f32x3_add(move, right); }
    if(RGFW_window_isKeyDown(window, RGFW_keyA)){ move = f32x3_sub(move, right); }
    if(RGFW_window_isKeyDown(window, RGFW_keyE) || RGFW_window_isMouseDown(window, RGFW_mouseLeft)){ move = f32x3_add(move, f32x3_up); }
    if(RGFW_window_isKeyDown(window, RGFW_keyQ) || RGFW_window_isMouseDown(window, RGFW_mouseRight)){ move = f32x3_sub(move, f32x3_up); }

    camera->position = f32x3_add(camera->position, f32x3_scale(f32x3_normalize(move), move_speed*dt));
}

b32
dot_game_init(DOT_Game *game, RN_RenderCtx *renderer, DOT_Window *window,
    u8 permanent_memory[], usize permanent_memory_size,
    u8 transient_memory[], usize transient_memory_size)
{
    g_game = game;
    g_game->renderer = renderer;
    g_game->window   = window;
    g_game->permanent_arena = ARENA_CREATE(.name = "engine permanent arena", .buffer = permanent_memory, .reserve_size = permanent_memory_size);
    g_game->transient_arena = ARENA_CREATE(.name = "engine transient arena", .buffer = transient_memory, .reserve_size = transient_memory_size);

    g_camera.position = v3(0.0f, 0.6f, 4.0f);
    g_camera.fov_y     = DOT_PI * 0.4f;
// NOTE: All this shouldn't be here but I am using this as a testbed for the render api for now
    RB_RasterDesc triangle_raster = {
        .raster_state.topology  = RB_PrimitiveTopologyKind_TriangleList,
        .color_targets          = { {.format = renderer->swapchain_format} },
        .depth_format           = renderer->depth_format,
        .debug_name             = str_comp("Triangle"),
    };


    g_triangle_pipeline = rn_pipeline_create(renderer,
        str_comp(DOT_GAME_SHADER_PATH"triangle.vert"),
        str_comp(DOT_GAME_SHADER_PATH"triangle.frag"),
        &triangle_raster);

    f32 h = 0.5f;
    TriangleVertex verts[] = {
        { .position = v3( 0.0f, -h, 0.0f), .color = v3(1,0,0) },
        { .position = v3(0.0f,   0.0f, 0.0f), .color = v3(0,1,0) },
        { .position = v3(-h,  0.0f, 0.0f), .color = v3(0,0,1) },

        { .position = v3( 0.0f, -h, 0.0f), .color = v3(1,0,0) },
        { .position = v3( -h,  -h, 0.0f), .color = v3(0,1,0) },
        { .position = v3(-h,  0, 0.0f), .color = v3(0,0,1) },

        { .position = v3( 0.0f, -h, 0.0f), .color = v3(1,0,0) },
        { .position = v3( 0,  -h, h), .color = v3(0,1,0) },
        { .position = v3(-h,  -h, h), .color = v3(0,0,1) },

        { .position = v3( 0.0f, -h, 0.0f), .color = v3(1,0,0) },
        { .position = v3( 0,  -h, h), .color = v3(0,1,0) },
        { .position = v3(-h,  -h, h), .color = v3(0,0,1) },
    };

    TriangleVertex cube_verts[36] = {
        // +X red
        {v3( h,-h,-h), v3(1,0,0)}, {v3( h, h,-h), v3(0,1,0)}, {v3( h, h, h), v3(0,0,1)},
        {v3( h,-h,-h), v3(1,0,0)}, {v3( h, h, h), v3(0,1,0)}, {v3( h,-h, h), v3(0,0,1)},
        // -X dark red
        {v3(-h,-h,-h), v3(0.5f,0,0)}, {v3(-h,-h, h), v3(0.5f,0,0)}, {v3(-h, h, h), v3(0.5f,0,0)},
        {v3(-h,-h,-h), v3(0.5f,0,0)}, {v3(-h, h, h), v3(0.5f,0,0)}, {v3(-h, h,-h), v3(0.5f,0,0)},
        // +Y green
        {v3(-h, h,-h), v3(0,1,0)}, {v3(-h, h, h), v3(0,1,0)}, {v3( h, h, h), v3(0,1,0)},
        {v3(-h, h,-h), v3(0,1,0)}, {v3( h, h, h), v3(0,1,0)}, {v3( h, h,-h), v3(0,1,0)},
        // -Y dark green
        {v3(-h,-h,-h), v3(0,0.5f,0)}, {v3( h,-h,-h), v3(0,0.5f,0)}, {v3( h,-h, h), v3(0,0.5f,0)},
        {v3(-h,-h,-h), v3(0,0.5f,0)}, {v3( h,-h, h), v3(0,0.5f,0)}, {v3(-h,-h, h), v3(0,0.5f,0)},
        // +Z blue
        {v3(-h,-h, h), v3(0,0,1)}, {v3( h,-h, h), v3(0,0,1)}, {v3( h, h, h), v3(0,0,1)},
        {v3(-h,-h, h), v3(0,0,1)}, {v3( h, h, h), v3(0,0,1)}, {v3(-h, h, h), v3(0,0,1)},
        // -Z dark blue
        {v3(-h,-h,-h), v3(0,0,0.5f)}, {v3(-h, h,-h), v3(0,0,0.5f)}, {v3( h, h,-h), v3(0,0,0.5f)},
        {v3(-h,-h,-h), v3(0,0,0.5f)}, {v3( h, h,-h), v3(0,0,0.5f)}, {v3( h,-h,-h), v3(0,0,0.5f)},
    };

    g_triangleMesh = rn_data_alloc(renderer, sizeof(verts));
    g_cubeMesh     = rn_data_alloc(renderer, sizeof(cube_verts));
    rn_upload_begin(renderer);
    rn_upload_memory(renderer, g_triangleMesh, verts, sizeof(verts));
    rn_upload_memory(renderer, g_cubeMesh, cube_verts, sizeof(cube_verts));
    rn_upload_end(renderer);

    RN_PSOGraphics cube_pso = {
        .vs_path = str_comp(DOT_GAME_SHADER_PATH"triangle.vert"),
        .fs_path = str_comp(DOT_GAME_SHADER_PATH"triangle.frag"),
        .raster_desc = {
            .raster_state.topology  = RB_PrimitiveTopologyKind_TriangleList,
            .raster_state.cull_mode = RB_CullModeKind_Back,
            .color_targets          = { {.format = renderer->swapchain_format} },
            .depth_format           = renderer->depth_format,
            .debug_name             = str_comp("Cube"),
        }
    };

    g_cube_pipeline = rn_graphics_pipeline_create(renderer, &cube_pso);

    // (claude) glTF front faces are CCW.
    RB_RasterDesc model_raster = {
        .raster_state.topology  = RB_PrimitiveTopologyKind_TriangleList,
        .raster_state.cull_mode = RB_CullModeKind_Back,
        .color_targets          = { {.format = renderer->swapchain_format} },
        .depth_format           = renderer->depth_format,
        .debug_name             = str_comp("Model"),
    };

    g_model_pipeline = rn_pipeline_create(renderer,
        str_comp(DOT_GAME_SHADER_PATH"model_bda.vert"),
        str_comp(DOT_GAME_SHADER_PATH"model_bda.frag"),
        &model_raster);

    // String8 model_path = str_comp(DOT_GAME_ASSET_PATH"glTF-Sample-Assets/Models/Cube/glTF/Cube.gltf");
    // String8 model_path = str_comp(DOT_GAME_ASSET_PATH"glTF-Sample-Assets/Models/Avocado/glTF/Avocado.gltf");
    String8 model_path = str_comp(DOT_GAME_ASSET_PATH"glTF-Sample-Assets/Models/Sponza/glTF/Sponza.gltf");
    g_model = dot_model_load_from_path(renderer, model_path);

    return true;
}

void
dot_game_run(DOT_Game *game)
{
    RN_RenderCtx *renderer = game->renderer;
    u32 current_frame = 0;
    f32 flash = cast(f32)((sin(cast(f32)current_frame / 1200.f)+1.f) / 2.0f);
    rn_clear_background(renderer, v3(0,0, cast(f32)flash));

    if(!renderer->current_frame_valid){ return; }

    local_persist u64 last_time_ns = 0;
    u64 now_ns = platform_get_time_ns();
    f32 dt = last_time_ns == 0 ? 0.0f : cast(f32)(now_ns - last_time_ns) / 1e9f;
    last_time_ns = now_ns;
    dot_camera_update(&g_camera, game->window, dt);

    f32 aspect = cast(f32)renderer->current_extent.x / cast(f32)renderer->current_extent.y;
    f32x4x4 view = f32x4x4_look_at(g_camera.position, f32x3_add(g_camera.position, dot_camera_forward(&g_camera)), f32x3_up);
    f32x4x4 proj = f32x4x4_perspective(g_camera.fov_y, aspect, 0.1f, 5000.0f);
    f32x4x4 view_proj = f32x4x4_mul(proj, view);

    RB_RenderingDesc desc = {
        .render_extent = renderer->current_extent,
        .color_attachments = {
            {
                .view     = renderer->current_view,
                .load_op  = RB_LoadOpKind_Load,
                .store_op = RB_StoreOpKind_Store,
            }
        },
        .depth_attachment = {
            .view        = renderer->depth_view,
            .load_op     = RB_LoadOpKind_Clear,
            .store_op    = RB_StoreOpKind_DontCare,
            .clear_value = 1.0f,
        },
    };

    // Orders this pass after the background clear (same color target) and the previous frame's
    // use of the shared depth texture.
    rb_barrier(renderer->current_cmd,
        RB_StageBit_ColorOutput | RB_StageBit_DepthStencilTests | RB_StageBit_Fragment,
        RB_AccessBit_ColorWrite | RB_AccessBit_DepthStencilWrite,
        RB_StageBit_ColorOutput | RB_StageBit_DepthStencilTests,
        RB_AccessBit_ColorRead | RB_AccessBit_ColorWrite | RB_AccessBit_DepthStencilRead | RB_AccessBit_DepthStencilWrite);

    rb_render_pass_begin(renderer->current_cmd, desc, RB_RenderingBit_None);
    rb_set_depth_stencil_state(renderer->current_cmd,
        &(RB_DepthStencilState){
            .depth_enable       = true,
            .depth_write_enable = true,
            .depth_comparison   = RB_CompareOpKind_Less,
        }
    );

    RB_Viewport viewport = {
        .viewport_rect = (RB_Extent2D16){
            .x = 0,
            .y = 0,
            .w = cast(u16)renderer->current_extent.x,
            .h = cast(u16)renderer->current_extent.y 
        },
        .max_depth = 1.0f,
    };

    rb_set_viewport(renderer->current_cmd, viewport);
    rb_set_scissor(renderer->current_cmd,
        (RB_Extent2D16){
            .x = 0,
            .y = 0,
            .w = cast(u16)renderer->current_extent.x,
            .h = cast(u16)renderer->current_extent.y
        }
    );

    rb_bind_pso(renderer->current_cmd, g_triangle_pipeline);

    TriangleRoot triangle_root = {0};
    triangle_root.mvp         = f32x4x4_identity();
    triangle_root.vertex_addr = g_triangleMesh;

    // rb_draw(renderer->current_cmd, rn_frame_push(renderer, triangle_root).gpu, 9, 1, 0, 0);

    // The Avocado sample is ~6 cm tall; scale it up to sit next to the unit cube.
    f32x4x4 model_world = f32x4x4_from_trs(f32x3_zero, quat_identity(), v3(20.0f, 20.0f, 20.0f));
    rb_bind_pso(renderer->current_cmd, g_model_pipeline);
    for EACH_INDEX(i, g_model.sub_mesh_draw_count){
        DOT_Submesh *sub_mesh = &g_model.sub_mesh_draw[i];

        DOT_ModelRoot model_root = {0};
        model_root.mvp           = f32x4x4_mul(view_proj, f32x4x4_mul(model_world, sub_mesh->transform));
        // model_root.mvp           = f32x4x4_identity();
        model_root.position_addr = sub_mesh->position_addr;
        model_root.normal_addr   = sub_mesh->normal_addr;
        model_root.texcoord_addr = sub_mesh->texcoord_addr;
        model_root.material_addr = sub_mesh->material_addr;
        RB_GpuRange indices = { .gpu = sub_mesh->index_addr, .size = cast(i64)(sub_mesh->index_count * sizeof(u32)) };
        rb_draw_indexed(renderer->current_cmd, rn_frame_push(renderer, model_root).gpu, indices, RB_IndexKind_U32, sub_mesh->index_count, 1, 0, 0, 0);
    }

    rb_bind_pso(renderer->current_cmd, g_cube_pipeline);
    TriangleRoot cube_root = {0};
    f32x4x4 cube_transform  = f32x4x4_from_trs(v3(1.5f, 0.5f, 0.0f), quat_identity(), f32x3_one);
    cube_root.mvp           = f32x4x4_mul(view_proj, cube_transform);
    cube_root.vertex_addr = g_cubeMesh;
    rb_draw(renderer->current_cmd, rn_frame_push(renderer, cube_root).gpu, 36, 1, 0, 0);

    rb_render_pass_end(renderer->current_cmd);
}

void dot_game_shutdown(DOT_Game* game)
{
    (void)game;
    dot_model_destroy(game->renderer, &g_model);
    rn_pipeline_destroy(game->renderer, g_model_pipeline);
    rn_pipeline_destroy(game->renderer, g_cube_pipeline);
    rn_pipeline_destroy(game->renderer, g_triangle_pipeline);
}

#endif
