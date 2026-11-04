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
#include "game/shaders/clouds_shared.h"

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

RB_Pipeline        g_clouds_pipeline = {0};
RN_BindlessTexture g_clouds_weather_map = {0};
RN_BindlessTexture g_clouds_height_density = {0};
RN_BindlessTexture g_clouds_noise = {0};
RN_BindlessTexture g_clouds_detail_noise = {0};
RN_BindlessTexture g_clouds_cirrus = {0};
u32                g_clouds_repeat_sampler_slot = 0;
u32                g_clouds_clamp_sampler_slot = 0;
u64                g_clouds_start_time_ns = 0;

DOT_CloudParams g_cloud_params = {
    .fast_march_transmittance_thresh = 0.01f,
    .min_marching_steps              = 35,
    .max_marching_steps              = 60,
    .stop_distance                   = 35000.0f,

    .max_steps_lights = 5,
    .light_step_short = 100.0f,
    .light_step_long  = 400.0f,

    .scattering_aniso_fw           = 0.8f,
    .scattering_aniso_bw           = 0.5f,
    .extinction_coefficient_sky    = 0.000015f,
    .scattering_coefficient_clouds = 0.3f,
    .scattering_coefficient_sugar  = 0.004f,

    .wind_speed = 10.0f,

    .map_density            = 1.0f,
    .clouds_min_height      = 1400.0f,
    .clouds_max_height      = 4000.0f,
    .weathermap_tile_radius = 50000.0f,

    .noise_weight             = 0.5f,
    .clouds_noise_scale       = 30000.0f,
    .clouds_noise_fbm_weights = { .x = 0.8f, .y = 0.32f, .z = 0.8f, .w = 1.0f },

    .detail_noise_weight      = 0.1f,
    .detail_noise_scale       = 1500.0f,
    .detail_noise_fbm_weights = { .x = 0.4f, .y = 0.8f, .z = 0.8f },

    .cirrus_height   = 10000.0f,
    .cirrus_scale    = 30000.0f,
    .cirrus_density  = 3.0f,
    .cirrus_coverage = 1.0f,

    .sky_color                    = { .x = 0.126f, .y = 0.307f, .z = 0.656f },
    .sky_brightness               = 1.0f,
    .brightness_gradient_strength = 2.5f,

    .sun_radius = 0.0002f,
};

DOT_SunLight g_sun = {
    .direction      = { .x = 0.0f, .y = 0.7f, .z = 0.7f },
    .energy         = 14.0f,
    .color          = { .x = 1.0f, .y = 0.9f, .z = 0.6f },
    .ambient_energy = 0.4f,
    .ambient_color  = { .x = 0.604f, .y = 0.604f, .z = 1.0f },
};

internal f32
dot_clouds_smoothstep(f32 edge0, f32 edge1, f32 x)
{
    f32 t = DOT_CLAMP01((x - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

internal f32
dot_clouds_hash01(u32 x, u32 y, u32 seed)
{
    u32 h = x * 73856093u ^ y * 19349663u ^ seed * 83492791u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return cast(f32)(h & 0xFFFFu) / 65535.0f;
}

internal RN_BindlessTexture
dot_clouds_texture_create(RN_RenderCtx *renderer, RB_TextureDimensionKind dimension_kind, RB_FormatKind format_kind, u16 width, u16 height, u16 depth, void *data, String8 name)
{
    RB_TextureDesc desc = {0};
    desc.dimension_kind = dimension_kind;
    desc.format_kind    = format_kind;
    desc.width          = width;
    desc.height         = height;
    desc.depth          = depth;
    desc.array_layers   = 1;
    desc.mip_levels     = 1;
    desc.debug_name     = name;
    return rn_texture_create_bindless(renderer, &desc, data);
}

internal void
dot_clouds_init(RN_RenderCtx *renderer)
{
    RB_RasterDesc clouds_raster = {
        .raster_state.topology  = RB_PrimitiveTopologyKind_TriangleList,
        .raster_state.cull_mode = RB_CullModeKind_None,
        .color_targets          = { {
            .format = renderer->hdr_format,
            .blend  = {
                .blend_enabled  = true,
                .separate_blend = true,
                .src_color      = RB_BlendFactorKind_One,
                .dst_color      = RB_BlendFactorKind_SrcAlpha,
                .blend_op       = RB_BlendOpKind_Add,
                .src_alpha      = RB_BlendFactorKind_Zero,
                .dst_alpha      = RB_BlendFactorKind_One,
                .alpha_blend_op = RB_BlendOpKind_Add,
            },
        } },
        .debug_name = str_comp("Clouds"),
    };
    g_clouds_pipeline = rn_pipeline_create(renderer,
        str_comp(RN_SHADER_PATH"fullscreen.vert"),
        str_comp(DOT_GAME_SHADER_PATH"clouds.frag"),
        &clouds_raster);

    RB_SamplerDesc repeat_sampler = {
        .min_filter     = RB_SamplerFilterKind_Linear,
        .mag_filter     = RB_SamplerFilterKind_Linear,
        .mipmap_filter  = RB_SamplerMipmapFilterKind_Linear,
        .address_mode_u = RB_SamplerAddressModeKind_Repeat,
        .address_mode_v = RB_SamplerAddressModeKind_Repeat,
        .address_mode_w = RB_SamplerAddressModeKind_Repeat,
        .debug_name     = str_comp("Clouds repeat"),
    };
    RB_SamplerDesc clamp_sampler = repeat_sampler;
    clamp_sampler.address_mode_u = RB_SamplerAddressModeKind_ClampToEdge;
    clamp_sampler.address_mode_v = RB_SamplerAddressModeKind_ClampToEdge;
    clamp_sampler.address_mode_w = RB_SamplerAddressModeKind_ClampToEdge;
    clamp_sampler.debug_name     = str_comp("Clouds clamp");
    g_clouds_repeat_sampler_slot = rn_sampler_bindless_slot_create(renderer, &repeat_sampler);
    g_clouds_clamp_sampler_slot  = rn_sampler_bindless_slot_create(renderer, &clamp_sampler);

    enum { WEATHER_MAP_SIZE = 256, WEATHER_MAP_CELLS = 8, HEIGHT_DENSITY_SIZE = 64 };
    TempArena temp = temp_arena_start(renderer->transient_arena);

    u8 *weather_map = PUSH_ARRAY(temp.arena, u8, WEATHER_MAP_SIZE * WEATHER_MAP_SIZE);
    for(u32 y = 0; y < WEATHER_MAP_SIZE; ++y){
        for(u32 x = 0; x < WEATHER_MAP_SIZE; ++x){
            f32 cell_u = (cast(f32)x + 0.5f) * cast(f32)WEATHER_MAP_CELLS / cast(f32)WEATHER_MAP_SIZE;
            f32 cell_v = (cast(f32)y + 0.5f) * cast(f32)WEATHER_MAP_CELLS / cast(f32)WEATHER_MAP_SIZE;
            u32 cell_x = cast(u32)cell_u;
            u32 cell_y = cast(u32)cell_v;
            f32 center_u = 0.35f + 0.3f * dot_clouds_hash01(cell_x, cell_y, 1);
            f32 center_v = 0.35f + 0.3f * dot_clouds_hash01(cell_x, cell_y, 2);
            f32 radius   = 0.15f + 0.2f * dot_clouds_hash01(cell_x, cell_y, 3);
            f32 du = cell_u - cast(f32)cell_x - center_u;
            f32 dv = cell_v - cast(f32)cell_y - center_v;
            f32 coverage = 1.0f - dot_clouds_smoothstep(radius * 0.4f, radius, sqrtf(du*du + dv*dv));
            weather_map[y * WEATHER_MAP_SIZE + x] = cast(u8)(coverage * 255.0f + 0.5f);
        }
    }

    u8 *height_density = PUSH_ARRAY(temp.arena, u8, HEIGHT_DENSITY_SIZE);
    for(u32 x = 0; x < HEIGHT_DENSITY_SIZE; ++x){
        f32 h = cast(f32)x / cast(f32)(HEIGHT_DENSITY_SIZE - 1);
        f32 density = dot_clouds_smoothstep(0.0f, 0.15f, h) * (1.0f - dot_clouds_smoothstep(0.4f, 1.0f, h));
        height_density[x] = cast(u8)(density * 255.0f + 0.5f);
    }

    g_clouds_weather_map    = dot_clouds_texture_create(renderer, RB_TextureDimensionKind_2D, RB_FormatKind_R8_Unorm, WEATHER_MAP_SIZE, WEATHER_MAP_SIZE, 1, weather_map, str_comp("Cloud weather"));
    g_clouds_height_density = dot_clouds_texture_create(renderer, RB_TextureDimensionKind_2D, RB_FormatKind_R8_Unorm, HEIGHT_DENSITY_SIZE, 1, 1, height_density, str_comp("Cloud height"));
    g_clouds_noise          = dot_clouds_texture_create(renderer, RB_TextureDimensionKind_3D, RB_FormatKind_Rgba8_Unorm, 1, 1, 1, (u8[]){255, 255, 255, 255}, str_comp("Clouds noise"));
    g_clouds_detail_noise   = dot_clouds_texture_create(renderer, RB_TextureDimensionKind_3D, RB_FormatKind_Rgba8_Unorm, 1, 1, 1, (u8[]){0, 0, 0, 0}, str_comp("Cloud detail"));
    g_clouds_cirrus         = dot_clouds_texture_create(renderer, RB_TextureDimensionKind_3D, RB_FormatKind_Rgba8_Unorm, 1, 1, 1, (u8[]){0, 0, 0, 0}, str_comp("Clouds cirrus"));

    temp_arena_restore(temp);
    g_clouds_start_time_ns = platform_get_time_ns();
}

internal void
dot_clouds_shutdown(RN_RenderCtx *renderer)
{
    rn_texture_destroy(renderer, g_clouds_weather_map.handle);
    rn_texture_destroy(renderer, g_clouds_height_density.handle);
    rn_texture_destroy(renderer, g_clouds_noise.handle);
    rn_texture_destroy(renderer, g_clouds_detail_noise.handle);
    rn_texture_destroy(renderer, g_clouds_cirrus.handle);
    rn_pipeline_destroy(renderer, g_clouds_pipeline);
}

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

internal void
dot_clouds_draw(RN_RenderCtx *renderer, DOT_Camera *camera, f32 aspect, f32 z_near, f32 z_far)
{
    f32x3 forward = dot_camera_forward(camera);
    f32x3 right   = f32x3_normalize(f32x3_cross(forward, f32x3_up));
    f32x3 up      = f32x3_cross(right, forward);
    f32   tan_half_fov_y = tanf(camera->fov_y * 0.5f);

    DOT_CloudsRoot root = {0};
    root.camera_position     = camera->position;
    root.time                = cast(f32)(platform_get_time_ns() - g_clouds_start_time_ns) / 1e9f;
    root.camera_forward      = forward;
    root.tan_half_fov_x      = tan_half_fov_y * aspect;
    root.camera_right        = right;
    root.tan_half_fov_y      = tan_half_fov_y;
    root.camera_up           = up;
    root.z_near              = z_near;
    root.z_far               = z_far;
    root.depth_slot          = renderer->depth_bindless_slot;
    root.repeat_sampler_slot = g_clouds_repeat_sampler_slot;
    root.clamp_sampler_slot  = g_clouds_clamp_sampler_slot;
    root.weather_map_slot    = g_clouds_weather_map.bindless_slot;
    root.height_density_slot = g_clouds_height_density.bindless_slot;
    root.noise_slot          = g_clouds_noise.bindless_slot;
    root.detail_noise_slot   = g_clouds_detail_noise.bindless_slot;
    root.cirrus_slot         = g_clouds_cirrus.bindless_slot;
    root.sun                 = g_sun;
    root.params              = g_cloud_params;

    rb_barrier(renderer->current_cmd,
        RB_StageBit_ColorOutput | RB_StageBit_DepthStencilTests,
        RB_AccessBit_ColorWrite | RB_AccessBit_DepthStencilWrite,
        RB_StageBit_Fragment | RB_StageBit_ColorOutput,
        RB_AccessBit_ShaderRead | RB_AccessBit_ColorRead | RB_AccessBit_ColorWrite);

    RB_RenderingDesc desc = {
        .render_extent = renderer->current_extent,
        .color_attachments = {
            {
                .view     = renderer->hdr_view,
                .load_op  = RB_LoadOpKind_Load,
                .store_op = RB_StoreOpKind_Store,
            }
        },
    };
    rb_render_pass_begin(renderer->current_cmd, desc, RB_RenderingBit_None);
    rb_set_depth_stencil_state(renderer->current_cmd, &(RB_DepthStencilState){0});
    rb_bind_pso(renderer->current_cmd, g_clouds_pipeline);

    renderer->tonemap_bypass = g_cloud_params.debug_view != DOT_CloudDebugView_Final
                            && g_cloud_params.debug_view != DOT_CloudDebugView_CloudColor;
    rb_draw(renderer->current_cmd, rn_frame_push(renderer, root).gpu, 3, 1, 0, 0);
    rb_render_pass_end(renderer->current_cmd);
}

enum { DOT_SHADOW_MAP_SIZE = 2048 };

Reflect(DOT_ShadowParams, {
    b32   enabled;
    f32x3 center;
    f32   half_extent;
    f32   depth_range;
    f32   depth_bias;
    f32   normal_offset_texels;
});

DOT_ShadowParams g_shadow_params = {
    .enabled              = true,
    .center               = { .x = 0.0f, .y = 100.0f, .z = 0.0f },
    .half_extent          = 330.0f,
    .depth_range          = 1200.0f,
    .depth_bias           = 0.0005f,
    .normal_offset_texels = 1.5f,
};

RN_BindlessTexture g_shadow_map = {0};
RB_RenderView      g_shadow_view = {0};
RB_Pipeline        g_shadow_pipeline = {0};

internal void
dot_shadow_init(RN_RenderCtx *renderer)
{
    RB_TextureDesc desc = {0};
    desc.dimension_kind = RB_TextureDimensionKind_2D;
    desc.format_kind    = renderer->depth_format;
    desc.width          = DOT_SHADOW_MAP_SIZE;
    desc.height         = DOT_SHADOW_MAP_SIZE;
    desc.depth          = 1;
    desc.array_layers   = 1;
    desc.mip_levels     = 1;
    desc.debug_name     = str_comp("Sun shadow map");
    g_shadow_map  = rn_texture_create_bindless(renderer, &desc, NULL);
    g_shadow_view = rb_render_view_create(g_shadow_map.handle, (RB_RenderViewDesc){0});

    RB_RasterDesc shadow_raster = {
        .raster_state.topology  = RB_PrimitiveTopologyKind_TriangleList,
        .raster_state.cull_mode = RB_CullModeKind_None,
        .depth_format           = renderer->depth_format,
        .debug_name             = str_comp("Sun shadow"),
    };
    g_shadow_pipeline = rn_pipeline_create(renderer,
        str_comp(DOT_GAME_SHADER_PATH"model_bda.vert"),
        (String8){0},
        &shadow_raster);
}

internal void
dot_shadow_shutdown(RN_RenderCtx *renderer)
{
    rn_pipeline_destroy(renderer, g_shadow_pipeline);
    rn_render_view_destroy(renderer, g_shadow_view);
    rn_texture_destroy(renderer, g_shadow_map.handle);
}

internal f32x4x4
dot_shadow_light_view_proj(DOT_ShadowParams *params, DOT_SunLight *sun)
{
    f32x3 to_sun = f32x3_normalize(sun->direction);
    f32x3 up     = fabsf(to_sun.y) > 0.99f ? v3(0.0f, 0.0f, 1.0f) : f32x3_up;
    f32x3 eye    = f32x3_add(params->center, f32x3_scale(to_sun, params->depth_range * 0.5f));
    f32x4x4 view = f32x4x4_look_at(eye, params->center, up);
    f32x4x4 proj = f32x4x4_orthographic(params->half_extent, params->half_extent, 0.0f, params->depth_range);
    return f32x4x4_mul(proj, view);
}

internal void
dot_shadow_draw(RN_RenderCtx *renderer, f32x4x4 light_view_proj, f32x4x4 model_world)
{
    RB_CommandBuffer cmd = renderer->current_cmd;
    rb_barrier(cmd,
        RB_StageBit_Fragment, RB_AccessBit_None,
        RB_StageBit_DepthStencilTests, RB_AccessBit_DepthStencilRead | RB_AccessBit_DepthStencilWrite);

    RB_RenderingDesc desc = {
        .render_extent    = (i32x2){ .x = DOT_SHADOW_MAP_SIZE, .y = DOT_SHADOW_MAP_SIZE },
        .depth_attachment = {
            .view        = g_shadow_view,
            .load_op     = RB_LoadOpKind_Clear,
            .store_op    = RB_StoreOpKind_Store,
            .clear_value = 1.0f,
        },
    };
    rb_render_pass_begin(cmd, desc, RB_RenderingBit_None);

    RB_Extent2D16 rect = { .x = 0, .y = 0, .w = DOT_SHADOW_MAP_SIZE, .h = DOT_SHADOW_MAP_SIZE };
    rb_set_viewport(cmd, (RB_Viewport){ .viewport_rect = rect, .max_depth = 1.0f });
    rb_set_scissor(cmd, rect);
    rb_set_depth_stencil_state(cmd,
        &(RB_DepthStencilState){
            .depth_enable       = true,
            .depth_write_enable = true,
            .depth_comparison   = RB_CompareOpKind_Less,
        }
    );
    rb_bind_pso(cmd, g_shadow_pipeline);
    for EACH_INDEX(i, g_model.sub_mesh_draw_count){
        DOT_Submesh *sub_mesh = &g_model.sub_mesh_draw[i];

        DOT_ModelRoot root = {0};
        root.world         = f32x4x4_mul(model_world, sub_mesh->transform);
        root.mvp           = f32x4x4_mul(light_view_proj, root.world);
        root.position_addr = sub_mesh->position_addr;
        root.normal_addr   = sub_mesh->normal_addr;
        root.texcoord_addr = sub_mesh->texcoord_addr;
        RB_GpuRange indices = { .gpu = sub_mesh->index_addr, .size = cast(i64)(sub_mesh->index_count * sizeof(u32)) };
        rb_draw_indexed(cmd, rn_frame_push(renderer, root).gpu, indices, RB_IndexKind_U32, sub_mesh->index_count, 1, 0, 0, 0);
    }
    rb_render_pass_end(cmd);

    rb_barrier(cmd,
        RB_StageBit_DepthStencilTests, RB_AccessBit_DepthStencilWrite,
        RB_StageBit_Fragment, RB_AccessBit_ShaderRead);
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
        .color_targets          = { {.format = renderer->hdr_format} },
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
            .color_targets          = { {.format = renderer->hdr_format} },
            .depth_format           = renderer->depth_format,
            .debug_name             = str_comp("Cube"),
        }
    };

    g_cube_pipeline = rn_graphics_pipeline_create(renderer, &cube_pso);

    RB_RasterDesc model_raster = {
        .raster_state.topology  = RB_PrimitiveTopologyKind_TriangleList,
        .raster_state.cull_mode = RB_CullModeKind_Back,
        .color_targets          = { {.format = renderer->hdr_format} },
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
    DOT_PROFILE_BLOCK("Model load"){ g_model = dot_model_load(renderer, model_path); }

    DOT_PROFILE_BLOCK("Clouds init"){ dot_clouds_init(renderer); }
    DOT_PROFILE_BLOCK("Shadow init"){ dot_shadow_init(renderer); }

    return true;
}

void
dot_game_run(DOT_Game *game)
{
    RN_RenderCtx *renderer = game->renderer;
    if(!renderer->current_frame_valid){ return; }

    local_persist u64 last_time_ns = 0;
    u64 now_ns = platform_get_time_ns();
    f32 dt = last_time_ns == 0 ? 0.0f : cast(f32)(now_ns - last_time_ns) / 1e9f;
    last_time_ns = now_ns;
    dot_camera_update(&g_camera, game->window, dt);

    f32 aspect = cast(f32)renderer->current_extent.x / cast(f32)renderer->current_extent.y;
    f32 z_near = 0.1f;
    f32 z_far  = 5000.0f;
    f32x4x4 view = f32x4x4_look_at(g_camera.position, f32x3_add(g_camera.position, dot_camera_forward(&g_camera)), f32x3_up);
    f32x4x4 proj = f32x4x4_perspective(g_camera.fov_y, aspect, z_near, z_far);
    f32x4x4 view_proj = f32x4x4_mul(proj, view);

    // The Avocado sample is ~6 cm tall; scale it up to sit next to the unit cube.
    f32x4x4 model_world     = f32x4x4_from_trs(f32x3_zero, quat_identity(), v3(20.0f, 20.0f, 20.0f));
    f32x4x4 light_view_proj = dot_shadow_light_view_proj(&g_shadow_params, &g_sun);
    if(g_shadow_params.enabled){
        dot_shadow_draw(renderer, light_view_proj, model_world);
    }

    DOT_ShadowGpu shadow_gpu = {
        .light_view_proj = light_view_proj,
        .shadow_slot     = g_shadow_map.bindless_slot,
        .map_size        = DOT_SHADOW_MAP_SIZE,
        .enabled         = g_shadow_params.enabled ? 1u : 0u,
        .depth_bias      = g_shadow_params.depth_bias,
        .normal_offset   = g_shadow_params.normal_offset_texels * 2.0f * g_shadow_params.half_extent / cast(f32)DOT_SHADOW_MAP_SIZE,
    };
    RB_GpuAddress shadow_addr = rn_frame_push(renderer, shadow_gpu).gpu;

    RB_RenderingDesc desc = {
        .render_extent = renderer->current_extent,
        .color_attachments = {
            {
                .view        = renderer->hdr_view,
                .load_op     = RB_LoadOpKind_Clear,
                .store_op    = RB_StoreOpKind_Store,
                .clear_color = v4(0.0f, 0.0f, 0.0f, 1.0f),
            }
        },
        .depth_attachment = {
            .view        = renderer->depth_view,
            .load_op     = RB_LoadOpKind_Clear,
            .store_op    = RB_StoreOpKind_Store,
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

    RB_GpuAddress sun_addr = rn_frame_push(renderer, g_sun).gpu;
    rb_bind_pso(renderer->current_cmd, g_model_pipeline);
    for EACH_INDEX(i, g_model.sub_mesh_draw_count){
        DOT_Submesh *sub_mesh = &g_model.sub_mesh_draw[i];

        DOT_ModelRoot model_root = {0};
        model_root.world         = f32x4x4_mul(model_world, sub_mesh->transform);
        model_root.mvp           = f32x4x4_mul(view_proj, model_root.world);
        // model_root.mvp           = f32x4x4_identity();
        model_root.position_addr = sub_mesh->position_addr;
        model_root.normal_addr   = sub_mesh->normal_addr;
        model_root.texcoord_addr = sub_mesh->texcoord_addr;
        model_root.material_addr = sub_mesh->material_addr;
        model_root.sun_addr      = sun_addr;
        model_root.shadow_addr   = shadow_addr;
        RB_GpuRange indices = { .gpu = sub_mesh->index_addr, .size = cast(i64)(sub_mesh->index_count * sizeof(u32)) };
        // TODO: (jd) change api to this?: 
        // rb_draw_indexed(cmd, root, sub_mesh->index_addr, RB_IndexKind_U32, sub_mesh->index_count, 1, 0, 0, 0);
        rb_draw_indexed(renderer->current_cmd, rn_frame_push(renderer, model_root).gpu, indices, RB_IndexKind_U32, sub_mesh->index_count, 1, 0, 0, 0);
    }

    rb_bind_pso(renderer->current_cmd, g_cube_pipeline);
    TriangleRoot cube_root = {0};
    f32x4x4 cube_transform  = f32x4x4_from_trs(v3(1.5f, 0.5f, 0.0f), quat_identity(), f32x3_one);
    cube_root.mvp           = f32x4x4_mul(view_proj, cube_transform);
    cube_root.vertex_addr = g_cubeMesh;
    rb_draw(renderer->current_cmd, rn_frame_push(renderer, cube_root).gpu, 36, 1, 0, 0);

    rb_render_pass_end(renderer->current_cmd);

    dot_clouds_draw(renderer, &g_camera, aspect, z_near, z_far);
}

void dot_game_shutdown(DOT_Game* game)
{
    (void)game;
    dot_shadow_shutdown(game->renderer);
    dot_clouds_shutdown(game->renderer);
    dot_model_destroy(game->renderer, &g_model);
    dot_asset_cache_destroy(game->renderer, &g_asset_cache);
    rn_pipeline_destroy(game->renderer, g_model_pipeline);
    rn_pipeline_destroy(game->renderer, g_cube_pipeline);
    rn_pipeline_destroy(game->renderer, g_triangle_pipeline);
}

#endif
