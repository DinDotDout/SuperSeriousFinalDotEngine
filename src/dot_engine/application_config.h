// NOTE: Rethink memory relationships
// For now I am manually encoding the total memory requirement relationships.
// Additions here mean that they do not own their memory and instead
// carve it out of the main arena
#define ENGINE_RENDERER_BACKEND_MEM_SIZE KB(128)
#define ENGINE_RENDERER_MEM_SIZE KB(256) + ENGINE_RENDERER_BACKEND_MEM_SIZE
#define APPLICATION_MEM_SIZE MB(1) + ENGINE_RENDERER_BACKEND_MEM_SIZE
#define APPLICATION_SCRATCH_MEM_SIZE KB(128)
#define PER_THREAD_TEMP_ARENA_COUNT 2

#define ENGINE_RENDERER_BACKEND RENDERER_BACKEND_VK

typedef struct ApplicationConfig {
    u64 thread_mem_size; // Split across N temp arenas
    u8  per_thread_temp_arena_count;
    u64 mem_size;
    RendererConfig renderer_config;
} ApplicationConfig;


// TODO: Add validation to ensure subregions fit when we start adding more memory requirements
internal ApplicationConfig*
application_config_get(){
    static ApplicationConfig app_config = {
        .mem_size                    = APPLICATION_MEM_SIZE,
        .thread_mem_size             = APPLICATION_SCRATCH_MEM_SIZE,
        .per_thread_temp_arena_count = PER_THREAD_TEMP_ARENA_COUNT,
        .renderer_config = (RendererConfig) {
            .mem_size         = ENGINE_RENDERER_MEM_SIZE,
            .frame_overlap    = 2,
            .frame_arena_size = KB(32),
            .backend_config = (RendererBackendConfig){
                .memory_size  = ENGINE_RENDERER_BACKEND_MEM_SIZE,
                .backend_kind = ENGINE_RENDERER_BACKEND,
            }
        },
    };
    return &app_config;
}
