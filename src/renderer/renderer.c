#include "base/arena.h"
#include "base/thread_ctx.h"
internal RendererBackend*
renderer_backend_create(Arena *arena, RendererBackendConfig *backend_config)
{
    Arena *backend_arena = ARENA_ALLOC(
        .parent = arena,
        .reserve_size = backend_config->backend_memory_size,
    );

    RendererBackend *base;
    switch (backend_config->backend_kind){
    case RendererBackendKind_Vk:   base = cast(RendererBackend*) renderer_backend_vk_create(backend_arena); break;
    case RendererBackendKind_Null: base = cast(RendererBackend*) renderer_backend_null_create(backend_arena); break;
    case RendererBackendKind_Gl: //base = cast(RendererBackend*) renderer_backend_gl_create(arena); break;
    case RendererBackendKind_Dx12: //base = cast(RendererBackend*) renderer_backend_dx_create(arena); break;
    default:
        DOT_ERROR("Unsupported renderer backend");
    }
    base->permanent_arena = arena;
    return base;
}

DOT_ShaderModule
renderer_load_shader_module_from_path(DOT_Renderer *renderer, String8 path)
{
    TempArena temp = threadctx_get_temp(0,0);
    FileBuffer file_buffer = platform_read_entire_file(temp.arena, path);
    DOT_ShaderModule shader_module = {
        .path = path,
        .shader_module_handle = renderer->backend->load_shader_module(renderer->backend, file_buffer),
    };
    temp_arena_restore(temp);
    return shader_module;
}

void
renderer_clear_background(DOT_Renderer *renderer, vec3 color)
{
    RendererBackend *backend = renderer->backend;
    u8 frame_idx = renderer->current_frame % renderer->frame_overlap;
    backend->clear_bg(renderer->backend, frame_idx, color);
}

internal void
renderer_init(Arena *arena, DOT_Renderer *renderer, DOT_Window *window, RendererConfig *renderer_config)
{
    renderer->permanent_arena = ARENA_ALLOC(
        .parent       = arena,
        .reserve_size = renderer_config->renderer_memory_size,
        .name         = "Application_Renderer");

    RendererBackend *backend = renderer_backend_create(renderer->permanent_arena, &renderer_config->backend_config);
    renderer->backend       = backend;
    renderer->frame_overlap = renderer_config->frame_overlap;
    renderer->frame_data    = PUSH_ARRAY(renderer->permanent_arena, FrameData, renderer->frame_overlap);
    for(u8 i = 0; i < renderer->frame_overlap; ++i){
        renderer->frame_data[i].temp_arena = ARENA_ALLOC(
            .parent       = renderer->permanent_arena,
            .reserve_size = renderer_config->frame_arena_size);
    }
    backend->init(backend, window);
}

internal void
renderer_shutdown(DOT_Renderer *renderer)
{
    RendererBackend *backend = renderer->backend;
    backend->shutdown(backend);
}

internal void
renderer_begin_frame(DOT_Renderer *renderer)
{
    RendererBackend *backend = renderer->backend;
    u8 frame_idx = renderer->current_frame % renderer->frame_overlap;
    backend->begin_frame(backend, frame_idx);
}

internal void
renderer_end_frame(DOT_Renderer *renderer)
{
    RendererBackend *backend = renderer->backend;
    u8 frame_idx = renderer->current_frame % renderer->frame_overlap;
    backend->end_frame(backend, frame_idx);
    ++renderer->current_frame;
}
