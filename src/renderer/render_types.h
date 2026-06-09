#ifndef RENDER_TYPES_H
#define RENDER_TYPES_H

////////////////////////////////////////////////////////////////
/// RenderTypes_TEXTURE

#define RENDER_TYPES_TEXTURE_DIMENSION_KINDS(X) \
    X(RenderTypes_TextureDimensionKind, 2D) \
    X(RenderTypes_TextureDimensionKind, 1D) \
    X(RenderTypes_TextureDimensionKind, 3D) \
    X(RenderTypes_TextureDimensionKind, Array1D) \
    X(RenderTypes_TextureDimensionKind, Array2D) \
    X(RenderTypes_TextureDimensionKind, Array3D) \

DOT_ENUM_REFLECT(RenderTypes_TextureDimensionKind, RENDER_TYPES_TEXTURE_DIMENSION_KINDS)

#define RENDER_TYPES_TEXTURE_FORMAT_KINDS(X) \
    X(RenderTypes_TextureFormatKind, Invalid) \
    /* 8 BIT*/\
    X(RenderTypes_TextureFormatKind, RGBA8_SRGB) \
    X(RenderTypes_TextureFormatKind, R8_UNORM) \
    X(RenderTypes_TextureFormatKind, R8_UINT) \
    X(RenderTypes_TextureFormatKind, RG8_UNORM) \
    X(RenderTypes_TextureFormatKind, RGB8_UNORM) \
    X(RenderTypes_TextureFormatKind, RGB8_SRGB) \
    X(RenderTypes_TextureFormatKind, RGBA8_UNORM) \
    X(RenderTypes_TextureFormatKind, BGRA8_UNORM) \
    X(RenderTypes_TextureFormatKind, BGRA8_SRGB) \
    /* HDR */\
    X(RenderTypes_TextureFormatKind, R16F) \
    X(RenderTypes_TextureFormatKind, RG16F) \
    X(RenderTypes_TextureFormatKind, RGBA16F) \
    X(RenderTypes_TextureFormatKind, R32F) \
    X(RenderTypes_TextureFormatKind, RG32F) \
    X(RenderTypes_TextureFormatKind, RGBA32F) \
    /* Depth stencil */\
    X(RenderTypes_TextureFormatKind, D16) \
    X(RenderTypes_TextureFormatKind, D24S8) \
    X(RenderTypes_TextureFormatKind, D32F) \
    X(RenderTypes_TextureFormatKind, D32FS8) \
    /* Block compressed */ \
    X(RenderTypes_TextureFormatKind, BC1) \
    X(RenderTypes_TextureFormatKind, BC1_SRGB) \
    X(RenderTypes_TextureFormatKind, BC3) \
    X(RenderTypes_TextureFormatKind, BC3_SRGB) \
    X(RenderTypes_TextureFormatKind, BC7) \
    X(RenderTypes_TextureFormatKind, BC7_SRGB) \
    X(RenderTypes_TextureFormatKind, ETC2_RGB8) \
    X(RenderTypes_TextureFormatKind, ETC2_RGBA8)

DOT_ENUM_REFLECT(RenderTypes_TextureFormatKind, RENDER_TYPES_TEXTURE_FORMAT_KINDS)

// ----- Texture format flags -----
typedef u8 RenderTypes_TextureFormatFlags;
enum{
    RenderTypes_TextureFormatBit_None         = DOT_BIT(0),
    RenderTypes_TextureFormatBit_Compressed   = DOT_BIT(1),
    RenderTypes_TextureFormatBit_SRGB         = DOT_BIT(2),
    RenderTypes_TextureFormatBit_Depth        = DOT_BIT(3),
    RenderTypes_TextureFormatBit_Stencil      = DOT_BIT(4),
};

// ----- Texture usage flags -----
typedef u8 RenderTypes_TextureUsageFlags;
enum{
    RenderTypes_TextureUsageBit_Default       = DOT_BIT(0),
    RenderTypes_TextureUsageBit_RenderTarget  = DOT_BIT(1),
    RenderTypes_TextureUsageBit_Compute       = DOT_BIT(2),
};

typedef struct RenderTypes_TextureFormatInfo{
    u8 channels;
    u8 bit_depth;
    u8 block_size;

    u8 block_width;
    u8 block_height;
    RenderTypes_TextureFormatFlags format_flags;
}RenderTypes_TextureFormatInfo;

typedef struct DOT_DescriptorSetLayoutHandle{
    DOT_AssetHandle handle;
}DOT_DescriptorSetLayoutHandle;

typedef struct DOT_TextureHandle{
    DOT_AssetHandle handle;
}DOT_TextureHandle;

global DOT_TextureHandle null_texture = {0};
typedef struct RenderTypes_TextureDesc{
    RenderTypes_TextureDimensionKind dimension_kind;
    RenderTypes_TextureFormatKind format_kind;
    RenderTypes_TextureUsageFlags texture_usage_flags;
    u16 width; // = 1;
    u16 height; // = 1;
    u16 depth; // = 1;
    u8 mip_levels; // = 1; // 0 will auto generate
}RenderTypes_TextureDesc;
#define RENDER_TYPES_TEXTURE_DESC(...) \
    &(RenderTypes_TextureDesc){ \
        .width = 1, \
        .height = 1, \
        .depth = 1, \
        .mip_levels = 1, \
        __VA_ARGS__ \
    }

// typedef struct RenderTypes_TextureCreateInfo{
//     void *data;
//     String8 debug_name;
//     RenderTypes_TextureDesc texture_desc; // This filled in by the texture loaded
// }RenderTypes_TextureCreateInfo;

typedef struct DOT_TextureAsset{
    DOT_Asset asset;
    DOT_TextureHandle handle;
    RenderTypes_TextureDesc desc;
}DOT_TextureAsset;

////////////////////////////////////////////////////////////////
/// RenderTypes_BUFFER

// ----- Buffer usage flags -----
typedef u8 RenderTypes_ResourceUsageKind;
enum{
    RenderTypes_ResourceUsageKind_Default,
    RenderTypes_ResourceUsageKind_Dynamic,
    RenderTypes_ResourceUsageKind_Readback,
    RenderTypes_ResourceUsageKind_GPUOnly = RenderTypes_ResourceUsageKind_Default,
};

typedef u8 RenderTypes_BufferUsageFlags;
enum{
    RenderTypes_BufferUsageBit_Vertex            = DOT_BIT(0),
    RenderTypes_BufferUsageBit_Index             = DOT_BIT(1),
    RenderTypes_BufferUsageBit_Uniform           = DOT_BIT(2),
    RenderTypes_BufferUsageBit_Storage           = DOT_BIT(3),
    RenderTypes_BufferUsageBit_Indirect          = DOT_BIT(4),
    RenderTypes_BufferUsageBit_DeviceAddress     = DOT_BIT(5),
};

typedef struct DOT_BufferHandle{
    DOT_AssetHandle handle;
}DOT_BufferHandle;

typedef struct RenderTypes_BufferDesc{
    u64 size;
    RenderTypes_BufferUsageFlags     buffer_usage_flags;
    RenderTypes_ResourceUsageKind    resource_usage;
}RenderTypes_BufferDesc;
#define RENDER_TYPES_BUFFER_DESC(...) \
    &(RenderTypes_BufferDesc){__VA_ARGS__} \

// typedef struct RenderTypes_BufferCreateInfo{
//     RenderTypes_AssetCreateInfo asset_info;
//     void *data; // must fill in desc manually
//                 //
//     // b32 create_mips; // Enabling this will auto fill mip_levels if no mip_levels
//
//     RenderTypes_BufferDesc buffer_desc; // This is now filled in by the texture loaded
//     // u8 flags; // = 0; // TextureFlags bitmasks
//     // TextureHandle alias = k_invalid_texture;
// }RenderTypes_BufferCreateInfo;

typedef struct DOT_BufferAsset{
    DOT_Asset asset;
    DOT_BufferHandle handle;
    RenderTypes_BufferDesc desc;
}DOT_BufferAsset;

////////////////////////////////////////////////////////////////
/// RenderTypes_SAMPLER

#define RENDER_TYPES_SAMPLER_FILTER_KINDS(X) \
    X(RenderTypes_SamplerFilterKind, Nearest) \
    X(RenderTypes_SamplerFilterKind, Linear) \
    X(RenderTypes_SamplerFilterKind, Cubic)

DOT_ENUM_REFLECT(RenderTypes_SamplerFilterKind, RENDER_TYPES_SAMPLER_FILTER_KINDS);

#define RENDER_TYPES_SAMPLER_MIP_MAP_MODE_KINDS(X) \
    X(RenderTypes_SamplerMipmapFilterKind, Nearest) \
    X(RenderTypes_SamplerMipmapFilterKind, Linear) \

DOT_ENUM_REFLECT(RenderTypes_SamplerMipmapFilterKind, RENDER_TYPES_SAMPLER_MIP_MAP_MODE_KINDS);

#define RENDER_TYPES_SAMPLER_ADDRESS_MODE_KINDS(X) \
    X(RenderTypes_SamplerAddressModeKind, Repeat) \
    X(RenderTypes_SamplerAddressModeKind, Mirrored_repeat) \
    X(RenderTypes_SamplerAddressModeKind, ClampToEdge) \
    X(RenderTypes_SamplerAddressModeKind, ClampToBorder) \
    X(RenderTypes_SamplerAddressModeKind, MirrorClampToEdge)
DOT_ENUM_REFLECT(RenderTypes_SamplerAddressModeKind, RENDER_TYPES_SAMPLER_ADDRESS_MODE_KINDS);

typedef struct DOT_SamplerHandle{
    DOT_AssetHandle handle;
}DOT_SamplerHandle;

typedef struct RenderTypes_SamplerDesc{
    RenderTypes_SamplerFilterKind       min_filter;
    RenderTypes_SamplerFilterKind       mag_filter;
    RenderTypes_SamplerMipmapFilterKind mipmap_filter;

    RenderTypes_SamplerAddressModeKind  address_mode_u;
    RenderTypes_SamplerAddressModeKind  address_mode_v;
    RenderTypes_SamplerAddressModeKind  address_mode_w;
}RenderTypes_SamplerDesc;
#define RENDER_TYPES_SAMPLER_DESC(...) \
    &(RenderTypes_SamplerDesc){ \
        __VA_ARGS__ \
    }

typedef struct DOT_SamplerAsset{
    DOT_Asset asset;
    DOT_SamplerHandle handle;
    RenderTypes_SamplerDesc desc;
}DOT_SamplerAsset;

// typedef struct RenderTypes_Program{
// }RenderTypes_Program;
enum{
    RENDER_TYPES_MAX_VERTEX_STREAMS = 16,
    RENDER_TYPES_MAX_VERTEX_ATTRIBUTES = 16
};
typedef enum RenderTypes_VertexInputRateKind{
    RenderTypes_VertexInputRateKind_PerVertex,
    RenderTypes_VertexInputRateKind_PerInstance,
    RenderTypes_VertexInputRateKind_Count,
}RenderTypes_VertexInputRateKind;

typedef struct RenderTypes_VertexStream{
    u16 binding;
    u16 stride;
    RenderTypes_VertexInputRateKind input_rate;
}RenderTypes_VertexStream;

typedef enum RenderTypes_VertexCommponentKind{
    RenderTypes_VertexCommponentKind_F32,
    RenderTypes_VertexCommponentKind_F32x2,
    RenderTypes_VertexCommponentKind_F32x3,
    RenderTypes_VertexCommponentKind_F32x4,
    RenderTypes_VertexCommponentKind_Mat4,
    RenderTypes_VertexCommponentKind_I8,
    RenderTypes_VertexCommponentKind_I8x4Norm,
    RenderTypes_VertexCommponentKind_U8,
    RenderTypes_VertexCommponentKind_U8x4Norm,
    RenderTypes_VertexCommponentKind_U16x2,
    RenderTypes_VertexCommponentKind_U16x2Norm,
    RenderTypes_VertexCommponentKind_U16x4,
    RenderTypes_VertexCommponentKind_U16x4Norm,
    RenderTypes_VertexCommponentKind_U32,
    RenderTypes_VertexCommponentKind_U32x2,
    RenderTypes_VertexCommponentKind_U32x4,
    RenderTypes_VertexCommponentKind_Count,
}RenderTypes_VertexCommponentKind;

typedef struct RenderTypes_VertexAttribute{
    u16 location;
    u16 binding;
    u16 offset;
    RenderTypes_VertexCommponentKind vertex_component_kind;
}RenderTypes_VertexAttribute;

typedef struct RenderTypes_VertexInput{
    ARRAY(RenderTypes_VertexAttribute, RENDER_TYPES_MAX_VERTEX_ATTRIBUTES) vertex_attributes;
    ARRAY(RenderTypes_VertexStream, RENDER_TYPES_MAX_VERTEX_STREAMS) vertex_streams;
}RenderTypes_VertexInput;

typedef u8 RenderTypes_CullModeFlags;
enum{
    RenderTypes_CullModeBit_Front   = DOT_BIT(0),
    RenderTypes_CullModeBit_Back    = DOT_BIT(1),
    RenderTypes_CullModeBit_FrontAndBack = RenderTypes_CullModeBit_Front | RenderTypes_CullModeBit_Back,
};

typedef enum RenderTypes_FrontFaceSortModeKind{
    RenderTypes_FrontFaceSortKind_CounterClockwise,
    RenderTypes_FrontFaceSortKind_Clockwise,
}RenderTypes_FrontFaceSortModeKind;

typedef enum RenderTypes_FillModeKind{
    RenderTypes_FillModeKind_Solid,
    RenderTypes_FillModeKind_Wireframe,
    RenderTypes_FillModeKind_Point,
    RenderTypes_FillModeKind_Count,
}RenderTypes_FillModeKind;

typedef struct RenderTypes_RasterState{
    RenderTypes_CullModeFlags cull_mode;
    RenderTypes_FrontFaceSortModeKind front_face_sort_mode;
    RenderTypes_FillModeKind fill_mode;
}RenderTypes_RasterState;

typedef enum RenderTypes_CompareOp{
    RenderTypes_CompareOp_Always,
    RenderTypes_CompareOp_Never,
    RenderTypes_CompareOp_Less,
    RenderTypes_CompareOp_Equal,
    RenderTypes_CompareOp_LessOrEqual,
    RenderTypes_CompareOp_Greater,
    RenderTypes_CompareOp_NotEqual,
    RenderTypes_CompareOp_GreaterOrEqual,
}RenderTypes_CompareOp;

typedef enum RenderTypes_StencilOp{
    RenderTypes_StencilOp_Keep,
    RenderTypes_StencilOp_Zero,
    RenderTypes_StencilOp_Replace,
    RenderTypes_StencilOp_IncrementAndClamp,
    RenderTypes_StencilOp_DecrementAndClamp,
    RenderTypes_StencilOp_Invert,
    RenderTypes_StencilOp_IncrementAndWrap,
    RenderTypes_StencilOp_DecrementAndWrap,
}RenderTypes_StencilOp;

typedef struct RenderTypes_StencilState{
    RenderTypes_StencilOp fail;
    RenderTypes_StencilOp pass;
    RenderTypes_StencilOp depth_fail;

    RenderTypes_CompareOp compare_op;

    u32 compare_mask;
    u32 write_mask;
    u32 reference_mask;
}RenderTypes_StencilState;

typedef struct RenderTypes_DepthStencilState{
    RenderTypes_StencilState front;
    RenderTypes_StencilState back;
    RenderTypes_CompareOp   depth_comparison;
    union{
        struct {
            b8 depth_enable : 1;
            b8 depth_write_enable : 1;
            b8 stencil_enable : 1;
        };
        u8 depth_stencil_bits;
    };
}RenderTypes_DepthStencilState;

enum
{
    RENDER_TYPES_IMAGE_OUTPUTS_MAX = 8, // Maximum number of images/render_targets/fbo attachments usable.
    RENDER_TYPES_DESCRIPTOR_SET_LAYOUT_MAX,
    RENDER_TYPES_SHADER_STAGES_MAX,
};

typedef enum RenderTypes_BlendFactorKind{
    RenderTypes_BlendFactorKind_Zero,
    RenderTypes_BlendFactorKind_One,
    RenderTypes_BlendFactorKind_SrcColor,
    RenderTypes_BlendFactorKind_OneMinusSrcColor,
    RenderTypes_BlendFactorKind_DstColor,
    RenderTypes_BlendFactorKind_OneMinusDstColor,
    RenderTypes_BlendFactorKind_SrcAlpha,
    RenderTypes_BlendFactorKind_OneMinusSrcAlpha,
    RenderTypes_BlendFactorKind_DstAlpha,
    RenderTypes_BlendFactorKind_OneMinusDstAlpha,
    RenderTypes_BlendFactorKind_ConstantColor,
    RenderTypes_BlendFactorKind_OneMinusConstantColor,
    RenderTypes_BlendFactorKind_ConstantAlpha,
    RenderTypes_BlendFactorKind_OneMinusConstantAlpha,
    RenderTypes_BlendFactorKind_SrcAlphaSaturate,
    RenderTypes_BlendFactorKind_Src1Color,
    RenderTypes_BlendFactorKind_OneMinusSrc1Color,
    RenderTypes_BlendFactorKind_Src1Alpha,
    RenderTypes_BlendFactorKind_OneMinusSrc1Alpha,
}RenderTypes_BlendFactorKind;

typedef enum RenderTypes_BlendOpKind{
    RenderTypes_BlendOpKind_Add,
    RenderTypes_BlendOpKind_Subtract,
    RenderTypes_BlendOpKind_ReverseSubtract,
    RenderTypes_BlendOpKind_Min,
    RenderTypes_BlendOpKind_Max,
}RenderTypes_BlendOpKind;

typedef u8 RenderTypes_ColorWriteFlags;
enum{
    RenderTypes_ColorWriteBit_Red   = DOT_BIT(0),
    RenderTypes_ColorWriteBit_Green = DOT_BIT(1),
    RenderTypes_ColorWriteBit_Blue  = DOT_BIT(2),
    RenderTypes_ColorWriteBit_Alpha = DOT_BIT(3),
    RenderTypes_ColorWriteBit_All = RenderTypes_ColorWriteBit_Red 
        | RenderTypes_ColorWriteBit_Green
        | RenderTypes_ColorWriteBit_Blue 
        | RenderTypes_ColorWriteBit_Alpha
};

typedef struct RenderTypes_BlendState{
    RenderTypes_BlendFactorKind src_color;
    RenderTypes_BlendFactorKind dest_color;
    RenderTypes_BlendOpKind     blend_op;

    RenderTypes_BlendFactorKind src_alpha;
    RenderTypes_BlendFactorKind dst_alpha;
    RenderTypes_BlendOpKind     alpha_blend_op;

    RenderTypes_ColorWriteFlags color_write_mask;
    b8 blend_enabled : 1;
    b8 separate_enabled : 1;
    b8 pad : 6;
}RenderTypes_BlendState;

typedef struct RenderTypes_RenderPassOutput{
    RenderTypes_TextureFormatKind depth_stencil_format;
    ARRAY(RenderTypes_TextureFormatKind, RENDER_TYPES_IMAGE_OUTPUTS_MAX) color_formats;
}RenderTypes_RenderPassOutput;

typedef struct RenderTypes_Extent2D16{
    i16 x, y;
    u16 w,h;
}RenderTypes_Extent2D16;

typedef struct RenderTypes_Viewport{
    RenderTypes_Extent2D16 viewport_rect;
    f32 min_depth;
    f32 max_depth;
}RenderTypes_Viewport;

typedef struct RenderTypes_ViewportState{
    u32 viewports_count;
    u32 scissors_count;
    RenderTypes_Viewport    *viewport;
    RenderTypes_Extent2D16  *scissors;

}RenderTypes_ViewportState;

typedef enum RenderTypes_ShaderStageKind{
    RenderTypes_ShaderStageKind_Vertex,
    RenderTypes_ShaderStageKind_Fragment,
    RenderTypes_ShaderStageKind_Compute,
    // RenderTypes_ShaderStageKind_AllGraphics,
    // RenderTypes_ShaderStageKind_All,
    RenderTypes_ShaderStageKind_RayGen,
    RenderTypes_ShaderStageKind_RayHitAny,
    RenderTypes_ShaderStageKind_RayHitClosest,
    RenderTypes_ShaderStageKind_RayHitMiss,
    RenderTypes_ShaderStageKind_RayHitIntersection,
    RenderTypes_ShaderStageKind_Mesh,

    RenderTypes_ShaderStageKind_TesselationControl,
    RenderTypes_ShaderStageKind_TesselationEvaluation,
    RenderTypes_ShaderStageKind_Geometry,
}RenderTypes_ShaderStageKind;

typedef enum RenderTypes_ShaderFormat{
    RenderTypes_ShaderFormat_Source,
    RenderTypes_ShaderFormat_Spirv,
    RenderTypes_ShaderFormat_Dxil
}RenderTypes_ShaderFormat;

typedef struct RenderTypes_ShaderStage{
    String8 code;
    RenderTypes_ShaderStageKind stage;
}RenderTypes_ShaderStage;

typedef struct RenderTypes_ShaderState{
    ARRAY(RenderTypes_ShaderStage, RENDER_TYPES_SHADER_STAGES_MAX) shader_stages;
    RenderTypes_ShaderFormat format;
    DOT_DEBUG_NAME(name, DOT_DEBUG_NAME_LEN);
}RenderTypes_ShaderState;

typedef struct RenderTypes_Pipeline{
    RenderTypes_RasterState             raster_state;
    RenderTypes_DepthStencilState       depth_stencil_state;
    RenderTypes_VertexInput             vertex_input;
    const RenderTypes_ViewportState     *viewport;
    RenderTypes_RenderPassOutput        render_pass;
    RenderTypes_ShaderState             shader_state;
    ARRAY(RenderTypes_BlendState, RENDER_TYPES_IMAGE_OUTPUTS_MAX) blend_states;
    ARRAY(DOT_DescriptorSetLayoutHandle, RENDER_TYPES_DESCRIPTOR_SET_LAYOUT_MAX) descriptor_set_layouts;
    DOT_DEBUG_NAME(name, DOT_DEBUG_NAME_LEN);
}RenderTypes_Pipeline;


internal RenderTypes_TextureFormatInfo renderer_texture_format_info_from_format(RenderTypes_TextureFormatKind fmt);
internal RenderTypes_TextureFormatKind renderer_texture_format_from_info(int comp, u8 size_bytes, b32 srgb);

#endif // !RENDER_TYPES_H
