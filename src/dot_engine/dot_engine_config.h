#ifndef APPLICATION_CONFIG_H
#define APPLICATION_CONFIG_H

#define DOT_ENGINE_ASSET_PATH  "assets/"

#define DOT_RENDER_BACKEND_ONLY_VK

// Some of the calculations aren't being translated properly. Revise memory allocs
DOT_CONST_INT_BLOCK{
    ENGINE_FRAME_OVERLAP = 3,
    ENGINE_FRAME_ARENA_SIZE = KB(32),

    ENGINE_RENDERER_BACKEND_MEM_SIZE = KB(512),
    ENGINE_RENDERER_TRANSIENT_MEM_SIZE = MB(1),

    ENGINE_RENDERER_PERMANENT_MEM_SIZE = KB(256) + ENGINE_RENDERER_TRANSIENT_MEM_SIZE +
        ENGINE_RENDERER_BACKEND_MEM_SIZE + (ENGINE_FRAME_ARENA_SIZE * ENGINE_FRAME_OVERLAP),

#if defined(DOT_RENDER_BACKEND_ONLY_DX12)
#   if DOT_OS_POSIX
#       error Unavaliable backend on linux!
#   endif
    ENGINE_RENDERER_BACKEND = RendererBackendKind_Dx12,
#elif defined(DOT_RENDER_BACKEND_ONLY_VK)
    ENGINE_RENDERER_BACKEND = RendererBackendKind_Vk,
#else
    ENGINE_RENDERER_BACKEND = RendererBackendKind_Auto,
#endif
    THREAD_TEMP_ARENA_COUNT = 2,
    THREAD_TEMP_ARENA_SIZE = MB(40),

    GAME_PERMANENT_MEMORY_SIZE = MB(20),
    GAME_TRANSIENT_MEMORY_SIZE = KB(32),

    APPLICATION_MEMORY_SIZE = MB(5) + ENGINE_RENDERER_PERMANENT_MEM_SIZE +
        GAME_PERMANENT_MEMORY_SIZE +
        GAME_TRANSIENT_MEMORY_SIZE,
};

#define DOT_MAKE_VERSION(major, minor, patch) \
    ((((u32)(major)) << 22U) | (((u32)(minor)) << 12U) | ((u32)(patch)))

// NOTE: Making this read only for now but we may want to add a bunch of function hooks
// or static vs dynamic config and update some things on the fly
typedef struct DOT_EngineConfig DOT_EngineConfig;
struct DOT_EngineConfig{
    u32              engine_version;
    u64              engine_memory_size;
    ThreadCtxOptions *thread_options;
    RendererConfig   *renderer_config;
    DOT_GameConfig   *game_config;
} global read_only DOT_ENGINE_CONFIG = {
    .engine_version     = DOT_MAKE_VERSION(0,0,0),
    .engine_memory_size = APPLICATION_MEMORY_SIZE,
    .thread_options = &(ThreadCtxOptions) {
        .per_thread_temp_arena_size  = THREAD_TEMP_ARENA_SIZE,
        .per_thread_temp_arena_count = THREAD_TEMP_ARENA_COUNT,
    },
    .game_config = &(DOT_GameConfig){
        .permanent_memory_size = GAME_PERMANENT_MEMORY_SIZE,
        .transient_memory_size = GAME_TRANSIENT_MEMORY_SIZE,
    },
    .renderer_config = &(RendererConfig){
        .renderer_transient_memory_size = ENGINE_RENDERER_TRANSIENT_MEM_SIZE,
        .renderer_permanent_memory_size = ENGINE_RENDERER_PERMANENT_MEM_SIZE,
        .frame_arena_size = ENGINE_FRAME_ARENA_SIZE,
        .backend_config = &(RendererBackendConfig){
            .backend_memory_size  = ENGINE_RENDERER_BACKEND_MEM_SIZE,
            .backend_kind = ENGINE_RENDERER_BACKEND,
            .present_mode = RendererPresentModeKind_Mailbox,
            .frame_overlap = ENGINE_FRAME_OVERLAP,
        },
        .shader_cache_config = &(ShaderCacheConfig){
            .shader_modules_count = SHADER_CACHE_SHADER_COUNT,
        }
    },
};

#endif // !APPLICATION_CONFIG_H
