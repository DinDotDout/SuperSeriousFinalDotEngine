#ifndef DOT_SHADER_H
#define DOT_SHADER_H

#define DOT_COMPILED_SHADER_PATH "src/game/compiled_shaders/"
#define DOT_RAW_TEXT(x) String8Lit(DOT_STR(x))

#define HM_IMPL
#define HM_T DOT_ShaderModule
#include "base/templates/hashmap.h"

internal read_only String8 g_shader_header = String8Lit(
"#version 450\n"
"#define PI 3.1415926538\n"
);

enum
{
    SHADER_CACHE_SHADER_COUNT = 64,
};

DOT_STATIC_ASSERT(IS_POW2(SHADER_CACHE_SHADER_COUNT), "Calculations assume a power of 2 for performance reasons");

typedef struct ShaderCacheConfig{
    u32 shader_modules_count;
}ShaderCacheConfig;

typedef struct DOT_ShaderModuleHandle{
    DOT_AssetHandle handle;
}DOT_ShaderModuleHandle;

typedef struct DOT_ShaderModule{
    DOT_Asset asset;
    DOT_ShaderModuleHandle shader_module_handle;
    String8 compiled_path;
}DOT_ShaderModule;


// NOTE: Since slang does not have a c interface, we will just call the binary for now
// I don't want to have to deal with glslang either and have multiple shader compilation
// backends
internal b32        shader_compile_from_path(String8 input_path, String8 output_path);
internal String8    shader_cache_get_compiled_path(Arena *arena, String8 path);
internal b32        shader_module_initialized(DOT_ShaderModule *shader_module);

#endif // !DOT_SHADER_H
