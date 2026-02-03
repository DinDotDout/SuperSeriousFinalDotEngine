// NOTE: Rethink memory relationships
// For now I am manually encoding the total memory requirement relationships.
// Additions here mean that they do not own their memory and instead
// carve it out of the main arena
#include "base/thread_ctx.h"
#define ENGINE_RENDERER_BACKEND_MEM_SIZE KB(128)
#define ENGINE_RENDERER_MEM_SIZE KB(256) + ENGINE_RENDERER_BACKEND_MEM_SIZE
#define APPLICATION_MEM_SIZE MB(1) + ENGINE_RENDERER_BACKEND_MEM_SIZE
#define APPLICATION_SCRATCH_MEM_SIZE KB(128)
#define PER_THREAD_TEMP_ARENA_COUNT 2

#define ENGINE_RENDERER_BACKEND RENDERER_BACKEND_VK

typedef struct ApplicationConfig {
    u32              app_version;
    ThreadCtxOptions thread_options;
    RendererConfig   renderer_config;
    u64              app_memory_size;
} ApplicationConfig;

#define DOT_MAKE_VERSION(major, minor, patch) \
    ((((u32)(major)) << 22U) | (((u32)(minor)) << 12U) | ((u32)(patch)))

// TODO: Add validation to ensure subregions fit when we start adding more memory requirements
internal ApplicationConfig*
application_config_get(){
    static ApplicationConfig app_config = {
        .app_version     = DOT_MAKE_VERSION(0,0,0),
        .app_memory_size = APPLICATION_MEM_SIZE,
        .thread_options = (ThreadCtxOptions) {
            .per_thread_temp_arena_size  = APPLICATION_SCRATCH_MEM_SIZE/PER_THREAD_TEMP_ARENA_COUNT,
            .per_thread_temp_arena_count = PER_THREAD_TEMP_ARENA_COUNT,
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
