#ifndef DOT_SHADER_H
#define DOT_SHADER_H

#define DOT_COMPILED_SHADER_PATH "src/game/compiled_shaders/"

CONST_INT_BLOCK ShaderCacheConfigValues{
    SHADER_CACHE_SHADER_COUNT = 64,
};
DOT_STATIC_ASSERT(IS_POW2(SHADER_CACHE_SHADER_COUNT));

// typedef struct DOT_Asset{
//     String8 name;
//     String8 path;
//     u64     last_modified;
// }DOT_Asset;

typedef struct ShaderCacheConfig{
    u32 shader_modules_count;
}ShaderCacheConfig;

typedef struct DOT_ShaderModuleHandle{
    u64 handle[1];
}DOT_ShaderModuleHandle;
read_only DOT_ShaderModuleHandle shader_module_handle_null = {0};

typedef struct DOT_ShaderModule{
    // DOT_Asset asset;
    DOT_ShaderModuleHandle shader_module_handle;
    String8 path;
    String8 compiled_path;
}DOT_ShaderModule;

typedef struct ShaderCacheNode ShaderCacheNode;
struct ShaderCacheNode{
    DOT_ShaderModule *shader_module;
    ShaderCacheNode  *next;
};

typedef u64 HashIdx;
typedef struct ShaderCache{
    hashset(ShaderCacheNode*) shader_modules;
    u32 shader_modules_count;
    u32 load_shader_modules;
}ShaderCache;

// NOTE: Since slang does not have a c interface, we will just call the binary for now
// I don't want to have to deal with glslang either and have multiple shader compilation
// backends
internal b8      shader_compile_from_path(String8 input_path, String8 output_path);

internal void    shader_cache_init(Arena *arena, ShaderCache *shader_cache, ShaderCacheConfig *shader_cache_config);
internal String8 shader_cache_get_compiled_path(Arena *arena, String8 path);
internal void    shader_cache_end(ShaderCache *shader_cache);
internal u64     shader_cache_hash(String8 shader_path);
internal HashIdx shader_cache_hash_idx(ShaderCache *shader_cache, String8 shader_path);
internal b8      shader_cache_is_null_shader(DOT_ShaderModule *shader_module);
internal void    shader_cache_push(Arena *arena, ShaderCache *shader_cache, DOT_ShaderModule *shader_module);
internal DOT_ShaderModule *shader_cache_get_or_create(
    Arena *arena,
    ShaderCache *shader_cache,
    String8 shader_path,
    String8 compiled_path);

internal DOT_ShaderModule* shader_module_create(Arena* arena, String8 path, String8 compiled_path);
internal b8                shader_module_initialized(DOT_ShaderModule *shader_module);

#endif // !DOT_SHADER_H
