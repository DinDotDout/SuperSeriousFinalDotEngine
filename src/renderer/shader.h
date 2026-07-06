#ifndef DOT_SHADER_H
#define DOT_SHADER_H

#define DOT_COMPILED_SHADER_PATH "src/game/compiled_shaders/"

internal read_only String8 g_shader_header = string8_lit(
"#version 450\n"
"#define PI 3.1415926538\n"
);

// (jd) NOTE: This allows embedding a text file. Tho it removes all line jumps
// also doesn't allow #define, hence g_shader_header
// This works for testing for now
#define DOT_RAW_TEXT(x) string8_lit( \
    "#version 450\n" \
    "#define PI 3.1415926538\n" \
    DOT_STR(x))

typedef struct RN_ShaderCachedData{
    RN_ShaderStageHandle shader_stage_handle;
    String8 compiled_path;
    String8 path;
    String8 byte_code;
}RN_ShaderCachedData;

#define HM_IMPL
#define HM_T RN_ShaderCachedData
#include "base/templates/hashmap.h"

// NOTE: Since slang does not have a c interface, we will just call the binary for now
// I don't want to have to deal with glslang either and have multiple shader compilation
// backends
internal b32        rn_shader_compile_from_path(u32 argument_count, String8 arguments[], String8 input_path, String8 output_path);
internal String8    rn_shader_cache_get_compiled_path(Arena *arena, String8 path);
internal b32        rn_shader_module_is_initialized(RN_ShaderCachedData *shader_module);

#endif // !DOT_SHADER_H
