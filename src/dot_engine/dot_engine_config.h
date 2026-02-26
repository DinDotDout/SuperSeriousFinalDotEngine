#ifndef APPLICATION_CONFIG_H
#define APPLICATION_CONFIG_H

// NOTE: we should probably split this up
CONST_INT_BLOCK {
    ENGINE_RENDERER_BACKEND_MEM_SIZE = KB(128),
    ENGINE_RENDERER_MEM_SIZE = KB(256) + ENGINE_RENDERER_BACKEND_MEM_SIZE,
    ENGINE_RENDERER_BACKEND = RendererBackendKind_Vk,

    THREAD_TEMP_ARENA_COUNT = 2,
    THREAD_SCRATCH_MEM_SIZE = KB(128),

    GAME_PERMANENT_MEMORY_SIZE = MB(1),
    GAME_TRANSIENT_MEMORY_SIZE = KB(32),

    APPLICATION_MEMORY_SIZE = MB(1) + ENGINE_RENDERER_BACKEND_MEM_SIZE +
                              GAME_PERMANENT_MEMORY_SIZE +
                              GAME_TRANSIENT_MEMORY_SIZE,
};

typedef struct DOT_EngineConfig {
    u32              engine_version;
    ThreadCtxOptions thread_options;
    RendererConfig   renderer_config;
    u64              engine_memory_size;
    DOT_GameConfig   game_config;
} DOT_EngineConfig;

#define DOT_MAKE_VERSION(major, minor, patch) \
    ((((u32)(major)) << 22U) | (((u32)(minor)) << 12U) | ((u32)(patch)))

internal DOT_EngineConfig*
dot_engine_config_get(){
    static DOT_EngineConfig app_config = {
        .engine_version     = DOT_MAKE_VERSION(0,0,0),
        .engine_memory_size = APPLICATION_MEMORY_SIZE,
        .thread_options = (ThreadCtxOptions) {
            .per_thread_temp_arena_size  = THREAD_SCRATCH_MEM_SIZE / THREAD_TEMP_ARENA_COUNT,
            .per_thread_temp_arena_count = THREAD_TEMP_ARENA_COUNT,
        },
        .game_config = (DOT_GameConfig){
            .permanent_memory_size = GAME_PERMANENT_MEMORY_SIZE,
            .transient_memory_size = GAME_TRANSIENT_MEMORY_SIZE,
        },
        .renderer_config = (RendererConfig){
            .renderer_memory_size = ENGINE_RENDERER_MEM_SIZE,
            .frame_arena_size = KB(32),
            .backend_config = (RendererBackendConfig){
                .backend_memory_size  = ENGINE_RENDERER_BACKEND_MEM_SIZE,
                .backend_kind = ENGINE_RENDERER_BACKEND,
                .present_mode = RendererPresentModeKind_Mailbox,
                .frame_overlap = 3,
            },
            .shader_cache_config = (ShaderCacheConfig){
                .shader_modules_count = SHADER_CACHE_SHADER_COUNT,
            }
        },
    };
    return &app_config;
}
#endif // !APPLICATION_CONFIG_H
