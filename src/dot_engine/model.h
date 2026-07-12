#ifndef DOT_MODEL_H
#define DOT_MODEL_H

typedef enum DOT_PrimitiveBufferKind{
    DOT_PrimitiveBufferKind_Index,
    DOT_PrimitiveBufferKind_Position,
    DOT_PrimitiveBufferKind_Tangent,
    DOT_PrimitiveBufferKind_Normal,
    DOT_PrimitiveBufferKind_Texcoord,
    DOT_PrimitiveBufferKind_Material,
    DOT_PrimitiveBufferKind_Count,
}DOT_PrimitiveBufferKind;

typedef enum DOT_PBRTextureKind{
    DOT_PBRTextureKind_Diffuse,
    DOT_PBRTextureKind_Normal,
    DOT_PBRTextureKind_Roughness,
    DOT_PBRTextureKind_Occlusion,
    DOT_PBRTextureKind_Emissive,
    DOT_PBRTextureKind_Count,
}DOT_PBRTextureKind;

typedef enum DOT_MaterialFeatureFlags{
    DOT_MaterialFeatureBit_ColorTexture     = DOT_BIT(0),
    DOT_MaterialFeatureBit_NormalTexture    = DOT_BIT(1),
    DOT_MaterialFeatureBit_RoughnessTexture = DOT_BIT(2),
    DOT_MaterialFeatureBit_OcclusionTexture = DOT_BIT(3),
    DOT_MaterialFeatureBit_EmissiveTexture  = DOT_BIT(4),

    DOT_MaterialFeatureBit_TangentVertexAttribute = DOT_BIT(5),
    DOT_MaterialFeatureBit_TexcoordVertexAttribute = DOT_BIT(6),
}DOT_MaterialFeatureFlags;

typedef struct alignas(16) DOT_MaterialPBR{
    // RN_BufferHandle buffers[DOT_PrimitiveBufferKind_Count];

    // RN_TextureHandle textures[DOT_PBRTextureKind_Count];
    // RN_SamplerHandle samplers[DOT_PBRTextureKind_Count];

    mat4 model_from_view;
    mat4 model_from_view_inv;

    vec4    base_color_factor;
    f32     metallic_factor;
    f32     roughness_factor;
    vec3    emissive_factor;
    f32     occlusion_factor;

    u32     flags;
}DOT_MaterialPBR;

typedef struct DOT_Vec3Array{
    u32     count;
    vec3    *gltf_data;
}DOT_Vec3Array;

typedef struct DOT_Vec2Array{
    u32     count;
    vec2    *gltf_data;
}DOT_Vec2Array;

typedef struct DOT_Primitive{
    u32  index_count;
    u32 *indices;

    DOT_Vec3Array positions;
    DOT_Vec3Array normals;
    DOT_Vec2Array uvs;

    u32 material_index;
}DOT_Primitive;

typedef struct DOT_Submesh{
    vec3 scale;

    u32 index_count;
    RN_BufferHandle buffers[DOT_PrimitiveBufferKind_Count];
    u32 buffer_offsets[DOT_PrimitiveBufferKind_Count];

    DOT_MaterialPBR material_data;

    // u32 mesh_index;   // index into scene.meshes
    mat4 transform;   // world matrix
    u32 material_index; // default material for this mesh (if you want per-mesh default)

    RN_ShaderResourceDesc shader_resource; // TODO: Move this where it makes sense
}DOT_Submesh;

typedef struct DOT_Scene{
    u32 sub_mesh_draw_count;
    DOT_Submesh *sub_mesh_draw;

    u32 sampler_count;
    RN_Sampler *samplers;
    u32 buffer_count;
    RN_Buffer  *buffers;
    u32 texture_count;
    RN_Texture *textures;
}DOT_Scene;

internal inline void*
dot_cgltf_alloc(void *user, cgltf_size size)
{
    Arena *arena = (Arena*)user;
    void *result = PUSH_SIZE(arena, size);
    // DOT_PRINT("allocating %u onto %p", size, result);
    return result;
}

internal inline void
dot_cgltf_free(void *user, void* ptr)
{
    // DOT_PRINT("freeing memory %p", ptr);
    DOT_UNUSED(user, ptr);
}

internal DOT_Scene dot_scene_from_gltf(RN_RenderCtx *renderer, const cgltf_data *gltf_data, String8 gltf_path);
internal cgltf_data * dot_gltf_load_from_path(Arena *arena, String8 path);
internal DOT_Scene dot_scene_load_from_path(RN_RenderCtx *renderer, String8 path);

#endif // DOT_MODEL_H!
