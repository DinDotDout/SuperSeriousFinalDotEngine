#define APPLICATION_MEM_SIZE MB(1)
#define APPLICATION_SCRATCH_MEM_SIZE KB(64)
#define ENGINE_RENDERER_MEM_SIZE KB(256)

#define ENGINE_RENDERER_BACKEND RENDERER_BACKEND_VK

typedef struct ApplicationConfig {
    u64 thread_mem_size; // Per arena (we have 2 arenas)
    u64 mem_size;
    RendererConfig renderer_config;
} ApplicationConfig;


// TODO: Add validation to ensure subregions fit when we start adding more memory requirements
internal ApplicationConfig application_config_get() {
    static ApplicationConfig app_config = {
        .mem_size = APPLICATION_MEM_SIZE,
        .thread_mem_size = APPLICATION_SCRATCH_MEM_SIZE,
        .renderer_config = (RendererConfig) {
            .mem_size = ENGINE_RENDERER_MEM_SIZE,
            .backend_kind = ENGINE_RENDERER_BACKEND,
        },
    };
    return app_config;
}
