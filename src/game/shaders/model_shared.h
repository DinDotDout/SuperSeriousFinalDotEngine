#ifndef MODEL_SHARED_H
#define MODEL_SHARED_H

ReflectEnum(u32, DOT_PBRTextureKind, {
    DOT_PBRTextureKind_Diffuse,
    DOT_PBRTextureKind_Normal,
    DOT_PBRTextureKind_Roughness,
    DOT_PBRTextureKind_Occlusion,
    DOT_PBRTextureKind_Emissive,
    DOT_PBRTextureKind_Count,
});

ReflectEnum(u32, DOT_MaterialFeatureFlags, {
    DOT_MaterialFeatureBit_ColorTexture     = 1 << 0,
    DOT_MaterialFeatureBit_NormalTexture    = 1 << 1,
    DOT_MaterialFeatureBit_RoughnessTexture = 1 << 2,
    DOT_MaterialFeatureBit_OcclusionTexture = 1 << 3,
    DOT_MaterialFeatureBit_EmissiveTexture  = 1 << 4,
});

Reflect(DOT_MaterialGpu, {
    f32x4 base_color_factor;
    f32x3 emissive_factor;
    f32   metallic_factor;
    f32   roughness_factor;
    f32   occlusion_factor;
    u32   flags;
    u32   tex_slots[DOT_PBRTextureKind_Count];
    u32   sampler_slots[DOT_PBRTextureKind_Count];
});

Reflect(DOT_ModelRoot, {
    f32x4x4       mvp;
    RB_GpuAddress position_addr;
    RB_GpuAddress normal_addr;
    RB_GpuAddress texcoord_addr;
    RB_GpuAddress material_addr;
});

#endif // !MODEL_SHARED_H
