#ifndef DOT_MODEL_H
#define DOT_MODEL_H

typedef struct DOT_Submesh{
    f32x4x4 transform;

    RB_GpuAddress index_addr;
    RB_GpuAddress position_addr;
    RB_GpuAddress normal_addr;
    RB_GpuAddress tangent_addr;    // 0 if the source mesh had no tangents
    RB_GpuAddress texcoord_addr;
    RB_GpuAddress material_addr;

    u32 index_count;
}DOT_Submesh;

typedef struct DOT_Model{
    u32 sub_mesh_draw_count;
    DOT_Submesh *sub_mesh_draw;

    u32 texture_count;
    RB_Texture *textures;
}DOT_Model;

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

internal DOT_Model      dot_model_from_gltf(RN_RenderCtx *renderer, const cgltf_data *gltf_data, String8 gltf_path);
internal cgltf_data *   dot_gltf_load_from_path(Arena *arena, String8 path);
internal DOT_Model      dot_model_load_from_path(RN_RenderCtx *renderer, String8 path);
internal void           dot_model_destroy(RN_RenderCtx *renderer, DOT_Model *model);

#endif // DOT_MODEL_H!
