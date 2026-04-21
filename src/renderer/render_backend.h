typedef struct RenderResource{
}RenderResource;

typedef enum RenderResourceKind{
    RenderResource_Texture,
    RenderResource_Sampler,
    RenderResource_Buffer,
    RenderResource_Count,

    // RenderResource_Material,
    // RenderResource_Technique,
}RenderResourceKind;


typedef struct RenderResourcePool{
    u8 pools[RenderResource_Count];
    u32 resource_idx[RenderResource_Count];
}RenderResourcePool;

DOT_CONST_INT_BLOCK {
    RenderResource_Max_Texture = U32_MAX,
    RenderResource_Max_Samplers = U32_MAX,
    RenderResource_Max_Buffers = U32_MAX,
};

// NOTE: could just make this dyn alloc
// RBVK_Texture textures[RenderResource_Max_Texture];
// RBVK_Sampler samplers[RenderResource_Max_Samplers];
// RBVK_Buffer buffers[RenderResource_Max_Buffers];
