#ifndef RN_H
#define RN_H

#define RN_PRESENT_MODE_KINDS(X) \
    X(RN_PresentModeKind, Immediate) \
    X(RN_PresentModeKind, Fifo) \
    X(RN_PresentModeKind, FifoRelaxed) \
    X(RN_PresentModeKind, Mailbox)
DOT_ENUM_REFLECT(RN_PresentModeKind, RN_PRESENT_MODE_KINDS)

////////////////////////////////////////////////////////////////
/// RN_TEXTURE

#define RN_TEXTURE_DIMENSION_KINDS(X) \
    X(RN_TextureDimensionKind, 2D) \
    X(RN_TextureDimensionKind, 1D) \
    X(RN_TextureDimensionKind, 3D) \
    X(RN_TextureDimensionKind, Array1D) \
    X(RN_TextureDimensionKind, Array2D) \
    X(RN_TextureDimensionKind, Array3D)
DOT_ENUM_REFLECT(RN_TextureDimensionKind, RN_TEXTURE_DIMENSION_KINDS)

#define RN_TEXTURE_FORMAT_KINDS(X) \
    X(RN_TextureFormatKind, Invalid) \
    /* 8 BIT*/\
    X(RN_TextureFormatKind, RGBA8_SRGB) \
    X(RN_TextureFormatKind, R8_UNORM) \
    X(RN_TextureFormatKind, R8_UINT) \
    X(RN_TextureFormatKind, RG8_UNORM) \
    X(RN_TextureFormatKind, RGB8_UNORM) \
    X(RN_TextureFormatKind, RGB8_SRGB) \
    X(RN_TextureFormatKind, RGBA8_UNORM) \
    X(RN_TextureFormatKind, BGRA8_UNORM) \
    X(RN_TextureFormatKind, BGRA8_SRGB) \
    /* HDR */\
    X(RN_TextureFormatKind, R16F) \
    X(RN_TextureFormatKind, RG16F) \
    X(RN_TextureFormatKind, RGBA16F) \
    X(RN_TextureFormatKind, R32F) \
    X(RN_TextureFormatKind, RG32F) \
    X(RN_TextureFormatKind, RGBA32F) \
    /* Depth stencil */\
    X(RN_TextureFormatKind, D16) \
    X(RN_TextureFormatKind, D24S8) \
    X(RN_TextureFormatKind, D32F) \
    X(RN_TextureFormatKind, D32FS8) \
    /* Block compressed */ \
    X(RN_TextureFormatKind, BC1) \
    X(RN_TextureFormatKind, BC1_SRGB) \
    X(RN_TextureFormatKind, BC3) \
    X(RN_TextureFormatKind, BC3_SRGB) \
    X(RN_TextureFormatKind, BC7) \
    X(RN_TextureFormatKind, BC7_SRGB) \
    X(RN_TextureFormatKind, ETC2_RGB8) \
    X(RN_TextureFormatKind, ETC2_RGBA8)

DOT_ENUM_REFLECT(RN_TextureFormatKind, RN_TEXTURE_FORMAT_KINDS)

// ----- Texture format flags -----
typedef u8 RN_TextureFormatFlags;
enum{
    RN_TextureFormatBit_None         = DOT_BIT(0),
    RN_TextureFormatBit_Compressed   = DOT_BIT(1),
    RN_TextureFormatBit_SRGB         = DOT_BIT(2),
    RN_TextureFormatBit_Depth        = DOT_BIT(3),
    RN_TextureFormatBit_Stencil      = DOT_BIT(4),
};

// ----- Texture usage flags -----
typedef u8 RN_TextureUsageFlags;
enum{
    RN_TextureUsageBit_Default       = DOT_BIT(0),
    RN_TextureUsageBit_RenderTarget  = DOT_BIT(1),
    RN_TextureUsageBit_Compute       = DOT_BIT(2),
};

typedef struct RN_TextureFormatInfo{
    u8 channels;
    u8 bit_depth;
    u8 block_size;

    u8 block_width;
    u8 block_height;
    RN_TextureFormatFlags format_flags;
}RN_TextureFormatInfo;


#define RN_SHADER_RESOURCE_LAYOUT_KINDS(X) \
        X(RN_ShaderResourceKind, Sampler) \
        X(RN_ShaderResourceKind, SampledTexture) /* SRV */ \
        X(RN_ShaderResourceKind, StorageTexture) /* UAV */ \
        X(RN_ShaderResourceKind, SamplerXTexture) /* ?? */ \
        X(RN_ShaderResourceKind, UniformBuffer)  /* CBV */ \
        X(RN_ShaderResourceKind, StorageBuffer)  /* SRV / UAV*/
DOT_ENUM_REFLECT(RN_ShaderResourceKind, RN_SHADER_RESOURCE_LAYOUT_KINDS);

enum{RN_SHADER_RESOURCE_BINDING_MAX = 10};
typedef struct RN_ShaderResourceBinding{
    RN_ShaderResourceKind resource_kind;
    u16 start;
    u16 count;
    u16 set;
    String8 name;
}RN_ShaderResourceBinding;

typedef struct RN_ShaderResourceLayout{
    u16 binding_count;
    RN_ShaderResourceBinding bindings[RN_SHADER_RESOURCE_BINDING_MAX];

    u16 set_index;

}RN_ShaderResourceLayout;

typedef struct DOT_DescriptorSetLayoutHandle{
    DOT_AssetHandle handle;
}DOT_DescriptorSetLayoutHandle;

typedef struct RN_TextureDesc{
    RN_TextureDimensionKind dimension_kind;
    RN_TextureFormatKind format_kind;
    RN_TextureUsageFlags texture_usage_flags;
    u16 width; // = 1;
    u16 height; // = 1;
    u16 depth; // = 1;
    u8 mip_levels; // = 1; // 0 will auto generate
}RN_TextureDesc;

#define RN_TEXTURE_DESC(...) \
    &(RN_TextureDesc){ \
        .width = 1, \
        .height = 1, \
        .depth = 1, \
        .mip_levels = 1, \
        __VA_ARGS__ \
    }

// typedef struct RN_TextureCreateInfo{
//     void *data;
//     String8 debug_name;
//     RN_TextureDesc texture_desc; // This filled in by the texture loaded
// }RN_TextureCreateInfo;

typedef struct DOT_TextureHandle{
    DOT_AssetHandle handle;
}DOT_TextureHandle;

global DOT_TextureHandle null_texture = {0};
typedef struct DOT_TextureAsset{
    DOT_Asset asset;
    DOT_TextureHandle handle;
    RN_TextureDesc desc;
}DOT_TextureAsset;

////////////////////////////////////////////////////////////////
/// RN_BUFFER

// ----- Buffer usage flags -----
typedef u8 RN_ResourceUsageKind;
enum{
    RN_ResourceUsageKind_Default,
    RN_ResourceUsageKind_Dynamic,
    RN_ResourceUsageKind_Readback,
    RN_ResourceUsageKind_GPUOnly = RN_ResourceUsageKind_Default,
};

typedef u8 RN_BufferUsageFlags;
enum{
    RN_BufferUsageBit_Vertex            = DOT_BIT(0),
    RN_BufferUsageBit_Index             = DOT_BIT(1),
    RN_BufferUsageBit_Uniform           = DOT_BIT(2),
    RN_BufferUsageBit_Storage           = DOT_BIT(3),
    RN_BufferUsageBit_Indirect          = DOT_BIT(4),
    RN_BufferUsageBit_DeviceAddress     = DOT_BIT(5),
};

typedef struct DOT_BufferHandle{
    DOT_AssetHandle handle;
}DOT_BufferHandle;

typedef struct RN_BufferDesc{
    u64 size;
    RN_BufferUsageFlags     buffer_usage_flags;
    RN_ResourceUsageKind    resource_usage;
}RN_BufferDesc;
#define RN_BUFFER_DESC(...) \
    &(RN_BufferDesc){__VA_ARGS__} \

// typedef struct RN_BufferCreateInfo{
//     RN_AssetCreateInfo asset_info;
//     void *data; // must fill in desc manually
//                 //
//     // b32 create_mips; // Enabling this will auto fill mip_levels if no mip_levels
//
//     RN_BufferDesc buffer_desc; // This is now filled in by the texture loaded
//     // u8 flags; // = 0; // TextureFlags bitmasks
//     // TextureHandle alias = k_invalid_texture;
// }RN_BufferCreateInfo;

typedef struct DOT_BufferAsset{
    DOT_Asset asset;
    DOT_BufferHandle handle;
    RN_BufferDesc desc;
}DOT_BufferAsset;

////////////////////////////////////////////////////////////////
/// RN_SAMPLER

#define RN_SAMPLER_FILTER_KINDS(X) \
    X(RN_SamplerFilterKind, Nearest) \
    X(RN_SamplerFilterKind, Linear) \
    X(RN_SamplerFilterKind, Cubic)

DOT_ENUM_REFLECT(RN_SamplerFilterKind, RN_SAMPLER_FILTER_KINDS);

#define RN_SAMPLER_MIP_MAP_MODE_KINDS(X) \
    X(RN_SamplerMipmapFilterKind, Nearest) \
    X(RN_SamplerMipmapFilterKind, Linear) \

DOT_ENUM_REFLECT(RN_SamplerMipmapFilterKind, RN_SAMPLER_MIP_MAP_MODE_KINDS);

#define RN_SAMPLER_ADDRESS_MODE_KINDS(X) \
    X(RN_SamplerAddressModeKind, Repeat) \
    X(RN_SamplerAddressModeKind, Mirrored_repeat) \
    X(RN_SamplerAddressModeKind, ClampToEdge) \
    X(RN_SamplerAddressModeKind, ClampToBorder) \
    X(RN_SamplerAddressModeKind, MirrorClampToEdge)
DOT_ENUM_REFLECT(RN_SamplerAddressModeKind, RN_SAMPLER_ADDRESS_MODE_KINDS);

typedef struct DOT_SamplerHandle{
    DOT_AssetHandle handle;
}DOT_SamplerHandle;

typedef struct RN_SamplerDesc{
    RN_SamplerFilterKind       min_filter;
    RN_SamplerFilterKind       mag_filter;
    RN_SamplerMipmapFilterKind mipmap_filter;

    RN_SamplerAddressModeKind  address_mode_u;
    RN_SamplerAddressModeKind  address_mode_v;
    RN_SamplerAddressModeKind  address_mode_w;
}RN_SamplerDesc;
#define RN_SAMPLER_DESC(...) \
    &(RN_SamplerDesc){ \
        __VA_ARGS__ \
    }

typedef struct DOT_SamplerAsset{
    DOT_Asset asset;
    DOT_SamplerHandle handle;
    RN_SamplerDesc desc;
}DOT_SamplerAsset;

// typedef struct RN_Program{
// }RN_Program;
enum{
    RN_MAX_VERTEX_STREAMS = 16,
    RN_MAX_VERTEX_ATTRIBUTES = 16
};
typedef enum RN_VertexInputRateKind{
    RN_VertexInputRateKind_PerVertex,
    RN_VertexInputRateKind_PerInstance,
    RN_VertexInputRateKind_Count,
}RN_VertexInputRateKind;

typedef struct RN_VertexStream{
    u16 binding;
    u16 stride;
    RN_VertexInputRateKind input_rate;
}RN_VertexStream;

typedef enum RN_VertexCommponentKind{
    RN_VertexCommponentKind_F32,
    RN_VertexCommponentKind_F32x2,
    RN_VertexCommponentKind_F32x3,
    RN_VertexCommponentKind_F32x4,
    RN_VertexCommponentKind_Mat4,
    RN_VertexCommponentKind_I8,
    RN_VertexCommponentKind_I8x4Norm,
    RN_VertexCommponentKind_U8,
    RN_VertexCommponentKind_U8x4Norm,
    RN_VertexCommponentKind_U16x2,
    RN_VertexCommponentKind_U16x2Norm,
    RN_VertexCommponentKind_U16x4,
    RN_VertexCommponentKind_U16x4Norm,
    RN_VertexCommponentKind_U32,
    RN_VertexCommponentKind_U32x2,
    RN_VertexCommponentKind_U32x4,
    RN_VertexCommponentKind_Count,
}RN_VertexCommponentKind;

typedef struct RN_VertexAttribute{
    u16 location;
    u16 binding;
    u16 offset;
    RN_VertexCommponentKind vertex_component_kind;
}RN_VertexAttribute;

typedef struct RN_VertexInput{
    ARRAY(RN_VertexAttribute, RN_MAX_VERTEX_ATTRIBUTES) vertex_attributes;
    ARRAY(RN_VertexStream, RN_MAX_VERTEX_STREAMS) vertex_streams;
}RN_VertexInput;

typedef u8 RN_CullModeFlags;
enum{
    RN_CullModeBit_Front   = DOT_BIT(0),
    RN_CullModeBit_Back    = DOT_BIT(1),
    RN_CullModeBit_FrontAndBack = RN_CullModeBit_Front | RN_CullModeBit_Back,
};

typedef enum RN_FrontFaceSortModeKind{
    RN_FrontFaceSortKind_CounterClockwise,
    RN_FrontFaceSortKind_Clockwise,
}RN_FrontFaceSortModeKind;

typedef enum RN_FillModeKind{
    RN_FillModeKind_Solid,
    RN_FillModeKind_Wireframe,
    RN_FillModeKind_Point,
    RN_FillModeKind_Count,
}RN_FillModeKind;

typedef struct RN_RasterState{
    RN_CullModeFlags cull_mode;
    RN_FrontFaceSortModeKind front_face_sort_mode;
    RN_FillModeKind fill_mode;
}RN_RasterState;

typedef enum RN_CompareOp{
    RN_CompareOp_Always,
    RN_CompareOp_Never,
    RN_CompareOp_Less,
    RN_CompareOp_Equal,
    RN_CompareOp_LessOrEqual,
    RN_CompareOp_Greater,
    RN_CompareOp_NotEqual,
    RN_CompareOp_GreaterOrEqual,
}RN_CompareOp;

typedef enum RN_StencilOp{
    RN_StencilOp_Keep,
    RN_StencilOp_Zero,
    RN_StencilOp_Replace,
    RN_StencilOp_IncrementAndClamp,
    RN_StencilOp_DecrementAndClamp,
    RN_StencilOp_Invert,
    RN_StencilOp_IncrementAndWrap,
    RN_StencilOp_DecrementAndWrap,
}RN_StencilOp;

typedef struct RN_StencilState{
    RN_StencilOp fail;
    RN_StencilOp pass;
    RN_StencilOp depth_fail;

    RN_CompareOp compare_op;

    u32 compare_mask;
    u32 write_mask;
    u32 reference_mask;
}RN_StencilState;

typedef struct RN_DepthStencilState{
    RN_StencilState front;
    RN_StencilState back;
    RN_CompareOp   depth_comparison;
    union{
        struct {
            b8 depth_enable : 1;
            b8 depth_write_enable : 1;
            b8 stencil_enable : 1;
        };
        u8 depth_stencil_bits;
    };
}RN_DepthStencilState;

enum
{
    RN_IMAGE_OUTPUTS_MAX = 8, // Maximum number of images/render_targets/fbo attachments usable.
    RN_DESCRIPTOR_SET_LAYOUT_MAX,
    RN_SHADER_STAGES_MAX,
};

typedef enum RN_BlendFactorKind{
    RN_BlendFactorKind_Zero,
    RN_BlendFactorKind_One,
    RN_BlendFactorKind_SrcColor,
    RN_BlendFactorKind_OneMinusSrcColor,
    RN_BlendFactorKind_DstColor,
    RN_BlendFactorKind_OneMinusDstColor,
    RN_BlendFactorKind_SrcAlpha,
    RN_BlendFactorKind_OneMinusSrcAlpha,
    RN_BlendFactorKind_DstAlpha,
    RN_BlendFactorKind_OneMinusDstAlpha,
    RN_BlendFactorKind_ConstantColor,
    RN_BlendFactorKind_OneMinusConstantColor,
    RN_BlendFactorKind_ConstantAlpha,
    RN_BlendFactorKind_OneMinusConstantAlpha,
    RN_BlendFactorKind_SrcAlphaSaturate,
    RN_BlendFactorKind_Src1Color,
    RN_BlendFactorKind_OneMinusSrc1Color,
    RN_BlendFactorKind_Src1Alpha,
    RN_BlendFactorKind_OneMinusSrc1Alpha,
}RN_BlendFactorKind;

typedef enum RN_BlendOpKind{
    RN_BlendOpKind_Add,
    RN_BlendOpKind_Subtract,
    RN_BlendOpKind_ReverseSubtract,
    RN_BlendOpKind_Min,
    RN_BlendOpKind_Max,
}RN_BlendOpKind;

typedef u8 RN_ColorWriteFlags;
enum{
    RN_ColorWriteBit_Red   = DOT_BIT(0),
    RN_ColorWriteBit_Green = DOT_BIT(1),
    RN_ColorWriteBit_Blue  = DOT_BIT(2),
    RN_ColorWriteBit_Alpha = DOT_BIT(3),
    RN_ColorWriteBit_All = RN_ColorWriteBit_Red 
        | RN_ColorWriteBit_Green
        | RN_ColorWriteBit_Blue 
        | RN_ColorWriteBit_Alpha
};

typedef struct RN_BlendState{
    RN_BlendFactorKind src_color;
    RN_BlendFactorKind dest_color;
    RN_BlendOpKind     blend_op;

    RN_BlendFactorKind src_alpha;
    RN_BlendFactorKind dst_alpha;
    RN_BlendOpKind     alpha_blend_op;

    RN_ColorWriteFlags color_write_mask;
    b8 blend_enabled : 1;
    b8 separate_enabled : 1;
    b8 pad : 6;
}RN_BlendState;

typedef struct RN_RenderPassOutput{
    RN_TextureFormatKind depth_stencil_format;
    ARRAY(RN_TextureFormatKind, RN_IMAGE_OUTPUTS_MAX) color_formats;
}RN_RenderPassOutput;

typedef struct RN_Extent2D16{
    i16 x, y;
    u16 w,h;
}RN_Extent2D16;

typedef struct RN_Viewport{
    RN_Extent2D16 viewport_rect;
    f32 min_depth;
    f32 max_depth;
}RN_Viewport;

typedef struct RN_ViewportState{
    u32 viewports_count;
    u32 scissors_count;
    RN_Viewport    *viewport;
    RN_Extent2D16  *scissors;

}RN_ViewportState;

typedef enum RN_ShaderStageKind{
    RN_ShaderStageKind_Vertex,
    RN_ShaderStageKind_Fragment,
    RN_ShaderStageKind_Compute,
    // RN_ShaderStageKind_AllGraphics,
    // RN_ShaderStageKind_All,
    RN_ShaderStageKind_RayGen,
    RN_ShaderStageKind_RayHitAny,
    RN_ShaderStageKind_RayHitClosest,
    RN_ShaderStageKind_RayHitMiss,
    RN_ShaderStageKind_RayHitIntersection,
    RN_ShaderStageKind_Mesh,

    RN_ShaderStageKind_TesselationControl,
    RN_ShaderStageKind_TesselationEvaluation,
    RN_ShaderStageKind_Geometry,
}RN_ShaderStageKind;

typedef enum RN_ShaderFormat{
    RN_ShaderFormat_Source,
    RN_ShaderFormat_Spirv,
    RN_ShaderFormat_Dxil
}RN_ShaderFormat;

typedef struct RN_ShaderStage{
    String8 code;
    RN_ShaderStageKind stage;
}RN_ShaderStage;

typedef struct RN_ShaderState{
    ARRAY(RN_ShaderStage, RN_SHADER_STAGES_MAX) shader_stages;
    RN_ShaderFormat format;
    DOT_DEBUG_NAME(name, DOT_DEBUG_NAME_LEN);
}RN_ShaderState;

typedef struct RN_Pipeline{
    RN_RasterState             raster_state;
    RN_DepthStencilState       depth_stencil_state;
    RN_VertexInput             vertex_input;
    const RN_ViewportState     *viewport;
    RN_RenderPassOutput        render_pass;
    RN_ShaderState             shader_state;
    ARRAY(RN_BlendState, RN_IMAGE_OUTPUTS_MAX) blend_states;
    ARRAY(DOT_DescriptorSetLayoutHandle, RN_DESCRIPTOR_SET_LAYOUT_MAX) descriptor_set_layouts;
    DOT_DEBUG_NAME(name, DOT_DEBUG_NAME_LEN);
}RN_Pipeline;


internal RN_TextureFormatInfo rn_texture_format_info_from_format(RN_TextureFormatKind fmt);
internal RN_TextureFormatKind rn_texture_format_from_info(int comp, u8 size_bytes, b32 srgb);

#endif // !RN_H
