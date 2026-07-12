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

// (jd) Test ideas for reflection
// this relies on recompiling C so we cannot have hot reloading since the layout won't update
// with the new tyeps on the C side
#if 0
#ifdef DOT_GLSL
    // GLSL mode
    #define DOT_UINT   uint
    #define DOT_UVEC4  uvec4

    #define DOT_BEGIN_UNIFORM(name, set, binding) \
        layout(std140, set = set, binding = binding) uniform name {

    #define DOT_END_UNIFORM };

    // GLSL: DOT_FIELD expands ONLY to GLSL field
    #define DOT_FIELD(type, name) type name;
#else
    // C/C++ mode
    #include <stdint.h>
    #include <stddef.h>

    #define DOT_UINT   uint32_t
    #define DOT_UVEC4(name)  uint32_t name[4]

    #define DOT_BEGIN_UNIFORM(name, set, binding) \
        typedef struct name {

    #define DOT_END_UNIFORM } name;

    // C: DOT_FIELD expands to C field AND emits reflection metadata *outside* the struct
    #define DOT_FIELD(type, name) \
        type name; \
        DOT_EMIT_UNIFORM_METADATA(type, name)

    // Helper macro to emit metadata OUTSIDE the struct
    #define DOT_EMIT_UNIFORM_METADATA(type, name) \
        static const struct { const char* type; const char* name; size_t offset; size_t size; } \
        name##_uniform = { #type, #name, offsetof(LightingConstants, name), sizeof(type) };

#endif
// ---- Shared uniform block (fields declared ONCE) ----

// Common type aliases

// #define DOT_GLSL

// GLSL side
#define DOT_C

typedef struct RFL_ReflectMetaShader{
    RFL_ReflectMeta meta;
    u32 binding;
    u32 set;
}RFL_ReflectMetaShader;

#define ReflectShader(n, s, b, ...) \
    typedef struct n __VA_ARGS__ n; \
    global RFL_REFLECT_SECTION  RFL_ReflectMetaShader __rfl_g_reflected_##n= { \
        .meta = { \
            .kind = RFL_ReflectKind_Compound, \
            .name = string8_lit(#n), \
            .size = sizeof(n), \
            .align = DOT_ALIGNOF(n), \
            .fields = 0, \
            .hook = rfl_edit_hook_default, \
            .parse_data = string8_lit(#__VA_ARGS__), \
        }, \
        .binding = b, \
        .set = s, \
    }

#ifdef DOT_C
#   define ReflectUniform(name, s, b, BODY) ReflectShader(name, s, b, BODY alignas(16) )
#else
#   define ReflectUniform(name, s, b, BODY) layout(std140, set = s, binding = b) uniform name BODY
#endif

// This forces us to recompile C to get the reflection, so we are at a dead end
ReflectUniform(LightingConstants, 1, 1,
{
    uint textures[4];
    uint output_index;
    uint output_width;
    uint output_height;
    uint emissive_index;
});

// ReflectShader(LightingConstants, MATERIAL_SET, 1
//     DOT_UVEC4 textures;
//     DOT_UINT  output_index;
//     DOT_UINT  output_width;
//     DOT_UINT  output_height;
//     DOT_UINT  emissive_index;
// )
#endif
#endif // !DOT_SHADER_H
