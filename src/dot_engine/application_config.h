#ifndef APPLICATION_CONFIG_H
#define APPLICATION_CONFIG_H

CONST_INT_BLOCK{
    ENGINE_RENDERER_BACKEND_MEM_SIZE = KB(128),
    ENGINE_RENDERER_MEM_SIZE = KB(256) + ENGINE_RENDERER_BACKEND_MEM_SIZE,
    ENGINE_RENDERER_BACKEND = RendererBackendKind_Vk,

    THREAD_TEMP_ARENA_COUNT = 2,
    THREAD_SCRATCH_MEM_SIZE = KB(128),

    APPLICATION_MEM_SIZE = MB(1) + ENGINE_RENDERER_BACKEND_MEM_SIZE,
};

typedef struct ApplicationConfig {
    u32              app_version;
    ThreadCtxOptions thread_options;
    RendererConfig   renderer_config;
    u64              app_memory_size;
} ApplicationConfig;

#define DOT_MAKE_VERSION(major, minor, patch) \
    ((((u32)(major)) << 22U) | (((u32)(minor)) << 12U) | ((u32)(patch)))

internal ApplicationConfig*
application_config_get(){
    static ApplicationConfig app_config = {
        .app_version     = DOT_MAKE_VERSION(0,0,0),
        .app_memory_size = APPLICATION_MEM_SIZE,
        .thread_options = (ThreadCtxOptions) {
            .per_thread_temp_arena_size  = THREAD_SCRATCH_MEM_SIZE / THREAD_TEMP_ARENA_COUNT,
            .per_thread_temp_arena_count = THREAD_TEMP_ARENA_COUNT,
        },
        .renderer_config = (RendererConfig) {
            .renderer_memory_size = ENGINE_RENDERER_MEM_SIZE,
            .frame_overlap    = 2,
            .frame_arena_size = KB(32),
            .backend_config = (RendererBackendConfig){
                .backend_memory_size  = ENGINE_RENDERER_BACKEND_MEM_SIZE,
                .backend_kind = ENGINE_RENDERER_BACKEND,
            }
        },
    };
    return &app_config;
}
#endif // !APPLICATION_CONFIG_H
