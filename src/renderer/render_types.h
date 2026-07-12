#ifndef RN_H
#define RN_H

#define RN_RESOURCE_KINDS(X) \
    X(RN_ResourceKind, Unknown) \
    X(RN_ResourceKind, Texture) \
    X(RN_ResourceKind, Sampler) \
    X(RN_ResourceKind, Buffer) \
    X(RN_ResourceKind, Pipeline) \
    X(RN_ResourceKind, ShaderResourceLayout) \
    X(RN_ResourceKind, ShaderState) \
    X(RN_ResourceKind, Count)
DOT_ENUM_REFLECT_TYPED(u8, RN_ResourceKind, RN_RESOURCE_KINDS);

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
    X(RN_TextureFormatKind, D16_UNORM) \
    X(RN_TextureFormatKind, D24_UNORM_S8_UINT) \
    X(RN_TextureFormatKind, D32_SFLOAT) \
    X(RN_TextureFormatKind, D32F_S8_UINT) \
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
    X(RN_ShaderResourceKind, UniformBufferDynamic)  /* CBV */ \
    X(RN_ShaderResourceKind, StorageBuffer)  /* SRV / UAV*/
DOT_ENUM_REFLECT(RN_ShaderResourceKind, RN_SHADER_RESOURCE_LAYOUT_KINDS);

enum{RN_SHADER_RESOURCE_BINDING_MAX = 10};

typedef u64 RN_Handle;

internal inline u64
rn_handle_pack(u32 index, RN_ResourceKind kind)
{
    return ((u64)kind << 32) | (u64)index;
}

internal inline u32
rn_handle_index(u64 h)
{
    return cast(u32)(h & 0xffffffffu);
}

internal inline RN_ResourceKind
rn_handle_kind(u64 h)
{
    return cast(RN_ResourceKind)((h >> 32) & 0xffffu);
}

typedef struct RN_TextureHandle{
    RN_Handle handle;
}RN_TextureHandle;

typedef struct RN_ShaderResourceLayoutHandle{
    RN_Handle handle;
}RN_ShaderResourceLayoutHandle;

typedef struct RN_BufferHandle{
    RN_Handle handle;
}RN_BufferHandle;

typedef struct RN_SamplerHandle{
    RN_Handle handle;
}RN_SamplerHandle;

typedef struct RN_PipelineHandle{
    RN_Handle handle;
}RN_PipelineHandle;

typedef struct RN_ShaderStageHandle{
    RN_Handle handle;
}RN_ShaderStageHandle;

typedef struct RN_ShaderStateHandle{
    RN_Handle handle;
}RN_ShaderStateHandle;

typedef struct RN_ShaderResourceBinding{
    RN_ShaderResourceKind kind;
    u16 start;
    u16 count;
    String8 name;
    u16 set;
}RN_ShaderResourceBinding;

typedef struct RN_ShaderResourceHandle{
    RN_Handle handle;
}RN_ShaderResourceHandle;

typedef struct RN_ShaderResourceLayoutDesc{
    u16 binding_count;
    RN_ShaderResourceBinding bindings[RN_SHADER_RESOURCE_BINDING_MAX];

    u16 set_index;
}RN_ShaderResourceLayoutDesc;

typedef struct RN_ShaderResourceLayout{
    RN_ShaderResourceLayoutHandle handle;
    RN_ShaderResourceLayoutDesc desc;
}RN_ShaderResourceLayout;

typedef struct RN_ShaderResourceDesc{
    RN_ShaderResourceLayoutHandle layout_desc_h;

    u32 buffer_count;
    RN_BufferHandle     buffers[RN_SHADER_RESOURCE_BINDING_MAX];

    u32 texture_sampler_count;
    RN_TextureHandle    textures[RN_SHADER_RESOURCE_BINDING_MAX];
    RN_SamplerHandle    samplers[RN_SHADER_RESOURCE_BINDING_MAX];
}RN_ShaderResourceDesc;

typedef struct RN_TextureDesc{
    RN_TextureDimensionKind dimension_kind;
    RN_TextureFormatKind format_kind;
    RN_TextureUsageFlags texture_usage_flags;
    u16 width; // = 1;
    u16 height; // = 1;
    u16 depth; // = 1;
    u8 mip_levels; // = 1; // 0 will auto generate
    DOT_DebugName16(debug_name);
}RN_TextureDesc;

#define RN_TEXTURE_DESC(...) \
    &(RN_TextureDesc){ \
        .width = 1, \
        .height = 1, \
        .depth = 1, \
        .mip_levels = 1, \
        __VA_ARGS__ \
    }

global RN_TextureHandle null_texture = {0};
typedef struct RN_Texture{
    RN_TextureHandle handle;
    RN_TextureDesc desc;
}RN_Texture;

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

typedef struct RN_BufferDesc{
    u64 size;
    RN_BufferUsageFlags     buffer_usage_flags;
    RN_ResourceUsageKind    resource_usage;
    DOT_DebugName16(debug_name);
}RN_BufferDesc;

typedef struct RN_Buffer{
    RN_BufferHandle handle;
    RN_BufferDesc desc;
}RN_Buffer;

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
    X(RN_SamplerAddressModeKind, MirroredRepeat) \
    X(RN_SamplerAddressModeKind, ClampToEdge) \
    X(RN_SamplerAddressModeKind, ClampToBorder) \
    X(RN_SamplerAddressModeKind, MirrorClampToEdge)
DOT_ENUM_REFLECT(RN_SamplerAddressModeKind, RN_SAMPLER_ADDRESS_MODE_KINDS);


typedef struct RN_SamplerDesc{
    RN_SamplerFilterKind       min_filter;
    RN_SamplerFilterKind       mag_filter;
    RN_SamplerMipmapFilterKind mipmap_filter;

    RN_SamplerAddressModeKind  address_mode_u;
    RN_SamplerAddressModeKind  address_mode_v;
    RN_SamplerAddressModeKind  address_mode_w;
    DOT_DebugName16(debug_name);
}RN_SamplerDesc;

// #define RN_SAMPLER_DESC(...) \
//     &(RN_SamplerDesc){ \
//         __VA_ARGS__ \
//     }

typedef struct RN_Sampler{
    RN_SamplerHandle handle;
    RN_SamplerDesc desc;
}RN_Sampler;

// typedef struct RN_Program{
// }RN_Program;
enum{
    RN_VERTEX_STREAMS_MAX = 16,
    RN_VERTEX_ATTRIBUTES_MAX = 16
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

typedef enum RN_FormatKind{
    RN_FormatKind_F32,
    RN_FormatKind_F32x2,
    RN_FormatKind_F32x3,
    RN_FormatKind_F32x4,
    // RN_FormatKind_Mat4,
    RN_FormatKind_I8,
    RN_FormatKind_I8x4Norm,
    RN_FormatKind_U8,
    RN_FormatKind_U8x4Norm,
    RN_FormatKind_U16x2,
    RN_FormatKind_U16x2Norm,
    RN_FormatKind_U16x4,
    RN_FormatKind_U16x4Norm,
    RN_FormatKind_U32,
    RN_FormatKind_U32x2,
    RN_FormatKind_U32x4,
    RN_FormatKind_Count,
}RN_FormatKind;

typedef struct RN_VertexAttribute{
    u16 location;
    u32 binding;
    u32 offset;
    RN_FormatKind vertex_component_kind;
}RN_VertexAttribute;

typedef struct RN_VertexInput{
    u32 vertex_attribute_count;
    RN_VertexAttribute vertex_attributes[RN_VERTEX_ATTRIBUTES_MAX];

    u32 vertex_stream_count;
    RN_VertexStream vertex_streams[RN_VERTEX_STREAMS_MAX];
 
    // ARRAY(RN_VertexAttribute, RN_VERTEX_ATTRIBUTES_MAX) vertex_attributes;
    // ARRAY(RN_VertexStream, RN_VERTEX_STREAMS_MAX) vertex_streams;
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

typedef enum RN_CompareOpKind{
    RN_CompareOpKind_Always,
    RN_CompareOpKind_Never,
    RN_CompareOpKind_Less,
    RN_CompareOpKind_Equal,
    RN_CompareOpKind_LessOrEqual,
    RN_CompareOpKind_Greater,
    RN_CompareOpKind_NotEqual,
    RN_CompareOpKind_GreaterOrEqual,
}RN_CompareOpKind;

typedef enum RN_StencilOpKind{
    RN_StencilOpKind_Keep,
    RN_StencilOpKind_Zero,
    RN_StencilOpKind_Replace,
    RN_StencilOpKind_IncrementAndClamp,
    RN_StencilOpKind_DecrementAndClamp,
    RN_StencilOpKind_Invert,
    RN_StencilOpKind_IncrementAndWrap,
    RN_StencilOpKind_DecrementAndWrap,
}RN_StencilOpKind;

typedef struct RN_StencilState{
    RN_StencilOpKind fail;
    RN_StencilOpKind pass;
    RN_StencilOpKind depth_fail;

    RN_CompareOpKind compare_op;

    u32 compare_mask;
    u32 write_mask;
    u32 reference_mask;
}RN_StencilState;

typedef struct RN_DepthStencilState{
    RN_StencilState front;
    RN_StencilState back;
    RN_CompareOpKind   depth_comparison;
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
    RN_SHADER_RESOURCE_LAYOUT_MAX,
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
    RN_BlendFactorKind dst_color;
    RN_BlendOpKind     blend_op;

    RN_BlendFactorKind src_alpha;
    RN_BlendFactorKind dst_alpha;
    RN_BlendOpKind     alpha_blend_op;

    RN_ColorWriteFlags color_write_mask;
    b8 blend_enabled : 1;
    b8 separate_blend : 1;
    // b8 pad : 6;
}RN_BlendState;

typedef struct RN_RenderPassOutput{
    RN_TextureFormatKind depth_stencil_format;
    u32 color_format_count;
    RN_TextureFormatKind color_formats[RN_IMAGE_OUTPUTS_MAX];
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
    RN_ShaderStageKind_None , // (jd) Infer based on file type
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
    RN_ShaderStageKind_Callable,

    RN_ShaderStageKind_Mesh,
    RN_ShaderStageKind_Task,

    RN_ShaderStageKind_TesselationControl,
    RN_ShaderStageKind_TesselationEvaluation,
    RN_ShaderStageKind_Geometry,
    RN_ShaderStageKind_Count,
}RN_ShaderStageKind;
DOT_STATIC_ASSERT(RN_ShaderStageKind_Count == 15, "If this is increased, add ext_map entries and rn_shader_stage_from_ext");

typedef struct RN_ShaderStageExtMap{
    String8 ext;
    RN_ShaderStageKind kind;
}RN_ShaderStageExtMap;

#define RN_EXT_VERT    "vert"
#define RN_EXT_FRAG    "frag"
#define RN_EXT_COMP    "comp"

#define RN_EXT_RGEN    "rgen"
#define RN_EXT_RAHIT   "rahit"
#define RN_EXT_RCHIT   "rchit"
#define RN_EXT_RMISS   "rmiss"
#define RN_EXT_RINT    "rint"
#define RN_EXT_CALL    "call"

#define RN_EXT_MESH    "mesh"
#define RN_EXT_TASK    "task"

#define RN_EXT_TESC    "tesc"
#define RN_EXT_TESE    "tese"
#define RN_EXT_GEOM    "geom"

read_only RN_ShaderStageExtMap rn_g_shader_stage_ext_map[] = {
    { string8_lit(RN_EXT_VERT),  RN_ShaderStageKind_Vertex },
    { string8_lit(RN_EXT_FRAG),  RN_ShaderStageKind_Fragment },
    { string8_lit(RN_EXT_COMP),  RN_ShaderStageKind_Compute },

    { string8_lit(RN_EXT_RGEN),  RN_ShaderStageKind_RayGen },
    { string8_lit(RN_EXT_RAHIT), RN_ShaderStageKind_RayHitAny },
    { string8_lit(RN_EXT_RCHIT), RN_ShaderStageKind_RayHitClosest },
    { string8_lit(RN_EXT_RMISS), RN_ShaderStageKind_RayHitMiss },
    { string8_lit(RN_EXT_RINT),  RN_ShaderStageKind_RayHitIntersection },
    { string8_lit(RN_EXT_CALL),  RN_ShaderStageKind_Callable },

    { string8_lit(RN_EXT_MESH),  RN_ShaderStageKind_Mesh },
    { string8_lit(RN_EXT_TASK),  RN_ShaderStageKind_Task },

    { string8_lit(RN_EXT_TESC),  RN_ShaderStageKind_TesselationControl },
    { string8_lit(RN_EXT_TESE),  RN_ShaderStageKind_TesselationEvaluation },
    { string8_lit(RN_EXT_GEOM),  RN_ShaderStageKind_Geometry },
};

// typedef enum RN_ShaderFormat{
//     RN_ShaderFormat_Source,
//     RN_ShaderFormat_Spirv,
//     RN_ShaderFormat_Dxil
// }RN_ShaderFormat;

typedef struct RN_ShaderStage{
    String8 path;
    RN_ShaderStageKind stage_kind;
    RN_ShaderStageHandle shader_stage_handle;
}RN_ShaderStage;

typedef struct RN_ShaderState{
    u32 shader_stage_count;
    RN_ShaderStage shader_stages[RN_SHADER_STAGES_MAX];
    RN_ShaderStateHandle shader_state_handle;
    b8 is_graphics_pipeline;
    DOT_DebugName16(debug_name);
}RN_ShaderState;

typedef struct RN_PipelineDesc{
    RN_RasterState          raster_state;
    RN_DepthStencilState    depth_stencil_state;
    RN_VertexInput          vertex_input;
    RN_ViewportState        *viewport;
    RN_RenderPassOutput     render_pass;
    RN_ShaderState          shader_state;

    u32 blend_state_count;
    RN_BlendState blend_states[RN_IMAGE_OUTPUTS_MAX];
    // ARRAY(RN_BlendState, RN_IMAGE_OUTPUTS_MAX) blend_states;

    u32 shader_resource_layout_count;
    RN_ShaderResourceLayoutHandle shader_resource_layouts[RN_SHADER_RESOURCE_LAYOUT_MAX];
    DOT_DebugName16(debug_name);
}RN_PipelineDesc;

typedef struct RN_Pipeline{
    RN_PipelineHandle handle;
    RN_PipelineDesc desc;
}RN_Pipeline;

internal RN_TextureFormatInfo rn_texture_format_info_from_format(RN_TextureFormatKind fmt);
internal RN_TextureFormatKind rn_texture_format_from_info(int comp, u8 size_bytes, b32 srgb);

///////////////////////////////////////////////////////////////////
///
// RN_PipelineDesc
//

internal RN_PipelineDesc rn_pipeline_begin();
internal void rn_pipeline_set_vertex_attributes_(RN_PipelineDesc *pl, u32 count, RN_VertexAttribute *attr);
internal void rn_pipeline_set_vertex_streams_(RN_PipelineDesc *pl, u32 count, RN_VertexStream *streams);
internal void rn_pipeline_set_depth_stencil_state_(RN_PipelineDesc *pl, RN_DepthStencilState *depth_stencil);
internal void rn_pipeline_set_shader_state_(RN_PipelineDesc *pl, u32 count, RN_ShaderStage *stages);
internal void rn_pipeline_set_render_pass_output(RN_PipelineDesc *pl, RN_RenderPassOutput render_pass_output); // (jd) NOTE: full array copy but it's fine. Will probs move to slice anyway at some point
internal void rn_pipeline_push_shader_resource_layout(RN_PipelineDesc *, RN_ShaderResourceLayoutHandle);

#define rn_pipeline_set_vertex_attributes(pl, ...)      rn_pipeline_set_vertex_attributes_(pl, DOT_ARRAY_SPREAD_T(RN_VertexAttribute, __VA_ARGS__))
#define rn_pipeline_set_vertex_streams(pl, ...)         rn_pipeline_set_vertex_streams_(pl, DOT_ARRAY_SPREAD_T(RN_VertexStream, __VA_ARGS__))
#define rn_pipeline_set_depth_stencil_state(pl, ...)    rn_pipeline_set_depth_stencil_state_(pl, &(RN_DepthStencilState){__VA_ARGS__})
#define rn_pipeline_set_shader_state(pl,...)            rn_pipeline_set_shader_state_(pl, DOT_ARRAY_SPREAD_T(RN_ShaderStage, __VA_ARGS__))

///////////////////////////////////////////////////////////////////
//
// RN_ShaderResourceLayoutDesc
//

internal RN_ShaderResourceLayoutDesc    rn_shader_resource_layout_begin();
internal void                           rn_shader_resource_layout_set_bindings_(RN_ShaderResourceLayoutDesc *layout_desc, u16 binding_count, RN_ShaderResourceBinding *bindings);
internal void                           rn_shader_resource_layout_push_binding_(RN_ShaderResourceLayoutDesc *layout_desc, RN_ShaderResourceBinding *bindings);

#define rn_shader_resource_layout_set_bindings(pl, ...) rn_shader_resource_layout_set_bindings_(pl, DOT_ARRAY_SPREAD_T(RN_ShaderResourceBinding, __VA_ARGS__));
#define rn_shader_resource_layout_push_binding(pl, ...) rn_shader_resource_layout_push_binding_(pl,  &(RN_ShaderResourceBinding)__VA_ARGS__);
// internal void rn_pipeline_set_vertex_streams_(RN_ShaderResourceLayoutDesc *, u32 count, RN_VertexStream *streams);

///////////////////////////////////////////////////////////////////
//
// RN_ShaderResourceDesc
//

internal void rn_shader_resource_desc_set_layout(RN_ShaderResourceDesc *, RN_ShaderResourceLayoutHandle);
internal void rn_shader_resource_desc_bind_buffer(RN_ShaderResourceDesc *, RN_BufferHandle buffer, u16 binding);
internal void rn_shader_resource_desc_bind_texture(RN_ShaderResourceDesc *, RN_TextureHandle texture, u16 binding);
internal void rn_shader_resource_desc_bind_texture_sampler(RN_ShaderResourceDesc *, RN_TextureHandle texture, RN_SamplerHandle sampler, u16 binding);


///////////////////////////////////////////////////////////////////
// RN_ShaderStageKind
//

internal RN_ShaderStageKind rn_shader_stage_kind_from_ext(String8 ext);
internal RN_ShaderStageKind rn_shader_stage_kind_from_path(String8 path);

internal String8            rn_slang_stage_from_shader_stage_kind(RN_ShaderStageKind kind);

#endif // !RN_H
