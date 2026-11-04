#ifndef RB_DEVICE_H
#define RB_DEVICE_H

////////////////////////////////////////////////////////////////
/// RB_HANDLES

#define RB_HANDLE_DEFINE(object) typedef struct{u64 h;}object;
#define RB_HANDLE_VALID(handle) ((handle).h != 0)

RB_HANDLE_DEFINE(RB_Device);
RB_HANDLE_DEFINE(RB_Texture);
RB_HANDLE_DEFINE(RB_Pipeline);
RB_HANDLE_DEFINE(RB_CommandPool);
RB_HANDLE_DEFINE(RB_CommandBuffer);
RB_HANDLE_DEFINE(RB_RenderView);
RB_HANDLE_DEFINE(RB_TimelineSemaphore);
RB_HANDLE_DEFINE(RB_GpuHeap);
RB_HANDLE_DEFINE(RB_TextureHeap);

////////////////////////////////////////////////////////////////
/// RB_MEMORY

typedef u64 RB_GpuAddress;

typedef struct RB_GpuRange
{
    RB_GpuAddress gpu;
    i64           size;
}RB_GpuRange;

typedef struct RB_SizeAlign
{
    i64 size;
    i64 align;
}RB_SizeAlign;

typedef enum RB_MemoryKind
{
    RB_MemoryKind_GpuOnly,
    RB_MemoryKind_CpuToGpu, // CPU writes, GPU reads it in place (per-frame roots); prefers VRAM through BAR
    RB_MemoryKind_Staging,  // CPU writes, GPU only copies from it into GpuOnly memory; system RAM
    RB_MemoryKind_Readback,
    RB_MemoryKind_TextureDescriptorHeap,
    RB_MemoryKind_SamplerDescriptorHeap,
}RB_MemoryKind;

typedef enum RB_TextureDescriptorKind
{
    RB_TextureDescriptorKind_Sampled,
    RB_TextureDescriptorKind_Storage,
}RB_TextureDescriptorKind;

ReflectEnum(u32, RB_FormatKind,
{
    RB_FormatKind_None,

    RB_FormatKind_R8_Srgb,              // SRGB
    RB_FormatKind_Rg8_Srgb,
    RB_FormatKind_Rgba8_Srgb,
    RB_FormatKind_Bgra8_Srgb,

    RB_FormatKind_Rgba4_Unorm,          // Packed Unorm
    RB_FormatKind_R5g5b5a1_Unorm,
    RB_FormatKind_R5g6b5_Unorm,

    RB_FormatKind_R8_Unorm,             // Unorm
    RB_FormatKind_Rg8_Unorm,
    RB_FormatKind_Rgba8_Unorm,
    RB_FormatKind_Bgra8_Unorm,
    RB_FormatKind_R16_Unorm,
    RB_FormatKind_Rg16_Unorm,
    RB_FormatKind_Rgba16_Unorm,

    RB_FormatKind_R8_Uint,              // Uint
    RB_FormatKind_Rg8_Uint,
    RB_FormatKind_Rgba8_Uint,
    RB_FormatKind_Bgra8_Uint,
    RB_FormatKind_R16_Uint,
    RB_FormatKind_Rg16_Uint,
    RB_FormatKind_Rgba16_Uint,
    RB_FormatKind_R32_Uint,
    RB_FormatKind_Rg32_Uint,
    RB_FormatKind_Rgb32_Uint,
    RB_FormatKind_Rgba32_Uint,

    RB_FormatKind_R16_Float,             // Float
    RB_FormatKind_Rg16_Float,
    RB_FormatKind_Rgba16_Float,
    RB_FormatKind_R32_Float,
    RB_FormatKind_Rg32_Float,
    RB_FormatKind_Rgb32_Float,
    RB_FormatKind_Rgba32_Float,

    RB_FormatKind_Rgb10a2_Unorm,        // Special
    RB_FormatKind_Rg11b10_Float,        // Fast Hdr

    RB_FormatKind_D16_Unorm,            // Depth Stencil
    RB_FormatKind_D24_Unorm_S8_Uint,
    RB_FormatKind_D32_Float,
    RB_FormatKind_S8_Uint,
    RB_FormatKind_D32_Float_S8_Uint,

    RB_FormatKind_Eac_Rg,               // Compressed
    RB_FormatKind_Astc_4X4_Srgb,
    RB_FormatKind_Astc_4X4_Unorm,
    RB_FormatKind_Bc3_Srgb,
    RB_FormatKind_Bc3_Unorm,
    RB_FormatKind_Bc5_Rg,
    RB_FormatKind_Bc6h_UFloat,
    RB_FormatKind_Bc6h_SFloat,
    RB_FormatKind_Bc7_Srgb,
    RB_FormatKind_Bc7_Unorm,
});

////////////////////////////////////////////////////////////////
/// RB_TEXTURE

ReflectEnum(u32, RB_TextureDimensionKind,
{
    RB_TextureDimensionKind_2D,
    RB_TextureDimensionKind_1D,
    RB_TextureDimensionKind_3D,
    RB_TextureDimensionKind_Array2D,
    RB_TextureDimensionKind_Cube,
    RB_TextureDimensionKind_CubeArray,
});

typedef u8 RB_TextureFormatFlags;
enum
{
    RB_TextureFormatBit_None         = DOT_BIT(0),
    RB_TextureFormatBit_Compressed   = DOT_BIT(1),
    RB_TextureFormatBit_Srgb         = DOT_BIT(2),
    RB_TextureFormatBit_Depth        = DOT_BIT(3),
    RB_TextureFormatBit_Stencil      = DOT_BIT(4),
};

typedef u8 RB_TextureUsageFlags;
enum
{
    RB_TextureUsageBit_Default       = DOT_BIT(0),
    RB_TextureUsageBit_RenderTarget  = DOT_BIT(1),
    RB_TextureUsageBit_Compute       = DOT_BIT(2),
};

typedef struct RB_TextureFormatInfo
{
    u8 block_size;
    RB_TextureFormatFlags format_flags;
}RB_TextureFormatInfo;

typedef struct RB_TextureDesc
{
    RB_TextureDimensionKind dimension_kind;
    RB_FormatKind format_kind;
    RB_TextureUsageFlags texture_usage_flags;
    u16 width; // = 1;
    u16 height; // = 1;
    u16 depth; // = 1;
    u16 array_layers; // = 1;
    u8 mip_levels; // = 1; // 0 will auto generate
    String8 debug_name;
}RB_TextureDesc;

typedef struct RB_TextureCopyDesc
{
    u32 mip_level;
    u32 slice;
    i32x3 offset;
    i32x3 extent;
}RB_TextureCopyDesc;

typedef struct RB_RenderViewDesc
{
    u32 mip_level;
    u32 slice;
}RB_RenderViewDesc;

internal RB_TextureFormatInfo rb_texture_format_info_from_format(RB_FormatKind fmt);
internal RB_FormatKind rb_texture_format_from_info(int comp, u8 size_bytes, b32 srgb);

////////////////////////////////////////////////////////////////
/// RB_SAMPLER

ReflectEnum(u32, RB_SamplerFilterKind,
{
    RB_SamplerFilterKind_Nearest,
    RB_SamplerFilterKind_Linear,
    RB_SamplerFilterKind_Cubic,
});

ReflectEnum(u32, RB_SamplerMipmapFilterKind,
{
    RB_SamplerMipmapFilterKind_Nearest,
    RB_SamplerMipmapFilterKind_Linear,
});

ReflectEnum(u32, RB_SamplerAddressModeKind,
{
    RB_SamplerAddressModeKind_Repeat,
    RB_SamplerAddressModeKind_MirroredRepeat,
    RB_SamplerAddressModeKind_ClampToEdge,
    RB_SamplerAddressModeKind_ClampToBorder,
    RB_SamplerAddressModeKind_MirrorClampToEdge,
});

typedef struct RB_SamplerDesc
{
    RB_SamplerFilterKind       min_filter;
    RB_SamplerFilterKind       mag_filter;
    RB_SamplerMipmapFilterKind mipmap_filter;

    RB_SamplerAddressModeKind  address_mode_u;
    RB_SamplerAddressModeKind  address_mode_v;
    RB_SamplerAddressModeKind  address_mode_w;
    String8 debug_name;
}RB_SamplerDesc;

////////////////////////////////////////////////////////////////
/// RB_PIPELINE

enum
{
    RB_IMAGE_OUTPUTS_MAX = 8, // Maximum number of images/render_targets/fbo attachments usable.
};

typedef enum RB_ShaderStageKind
{
    RB_ShaderStageKind_None ,
    RB_ShaderStageKind_Vertex,
    RB_ShaderStageKind_Fragment,
    RB_ShaderStageKind_Compute,
    RB_ShaderStageKind_RayGen,
    RB_ShaderStageKind_RayHitAny,
    RB_ShaderStageKind_RayHitClosest,
    RB_ShaderStageKind_RayHitMiss,
    RB_ShaderStageKind_RayHitIntersection,
    RB_ShaderStageKind_Callable,

    RB_ShaderStageKind_Mesh,
    RB_ShaderStageKind_Task,

    RB_ShaderStageKind_TesselationControl,
    RB_ShaderStageKind_TesselationEvaluation,
    RB_ShaderStageKind_Geometry,
    RB_ShaderStageKind_Count,
}RB_ShaderStageKind;

typedef enum RB_FrontFaceSortModeKind
{
    RB_FrontFaceSortKind_CounterClockwise,
    RB_FrontFaceSortKind_Clockwise,
}RB_FrontFaceSortModeKind;

typedef u8 RB_CullModeKind;
enum{
    RB_CullModeKind_None,
    RB_CullModeKind_Front,
    RB_CullModeKind_Back,
    RB_CullModeKind_Always,
};

typedef enum RB_FillModeKind
{
    RB_FillModeKind_Solid,
    RB_FillModeKind_Wireframe,
    RB_FillModeKind_Point,

    RB_FillModeKind_Count,
}RB_FillModeKind;

typedef enum RB_PrimitiveTopologyKind
{
    RB_PrimitiveTopologyKind_TriangleList,
    RB_PrimitiveTopologyKind_TriangleStrip,
    RB_PrimitiveTopologyKind_LineList,
    RB_PrimitiveTopologyKind_PointList,
}RB_PrimitiveTopologyKind;

typedef struct RB_RasterState
{
    RB_CullModeKind cull_mode;

    f32 depth_bias_constant;
    f32 depth_bias_clamp;
    f32 depth_bias_slope;

    RB_FillModeKind fill_mode;
    RB_PrimitiveTopologyKind topology;
}RB_RasterState;

typedef enum RB_CompareOpKind
{
    RB_CompareOpKind_Always,
    RB_CompareOpKind_Never,
    RB_CompareOpKind_Less,
    RB_CompareOpKind_Equal,
    RB_CompareOpKind_LessOrEqual,
    RB_CompareOpKind_Greater,
    RB_CompareOpKind_NotEqual,
    RB_CompareOpKind_GreaterOrEqual,
}RB_CompareOpKind;

typedef enum RB_StencilOpKind
{
    RB_StencilOpKind_Keep,
    RB_StencilOpKind_Zero,
    RB_StencilOpKind_Replace,
    RB_StencilOpKind_IncrementAndClamp,
    RB_StencilOpKind_DecrementAndClamp,
    RB_StencilOpKind_Invert,
    RB_StencilOpKind_IncrementAndWrap,
    RB_StencilOpKind_DecrementAndWrap,
}RB_StencilOpKind;

typedef struct RB_StencilState
{
    RB_StencilOpKind fail;
    RB_StencilOpKind pass;
    RB_StencilOpKind depth_fail;

    RB_CompareOpKind compare_op;

    u32 compare_mask;
    u32 write_mask;
    u32 reference_mask;
}RB_StencilState;

typedef struct RB_DepthStencilState
{
    RB_StencilState front;
    RB_StencilState back;
    RB_CompareOpKind   depth_comparison;
    union{
        struct {
            b8 depth_enable         : 1;
            b8 depth_write_enable   : 1;
            b8 stencil_enable       : 1;
        };
        u8 depth_stencil_bits;
    };
}RB_DepthStencilState;

typedef enum RB_BlendFactorKind{
    RB_BlendFactorKind_Zero,
    RB_BlendFactorKind_One,
    RB_BlendFactorKind_SrcColor,
    RB_BlendFactorKind_OneMinusSrcColor,
    RB_BlendFactorKind_DstColor,
    RB_BlendFactorKind_OneMinusDstColor,
    RB_BlendFactorKind_SrcAlpha,
    RB_BlendFactorKind_OneMinusSrcAlpha,
    RB_BlendFactorKind_DstAlpha,
    RB_BlendFactorKind_OneMinusDstAlpha,
    RB_BlendFactorKind_ConstantColor,
    RB_BlendFactorKind_OneMinusConstantColor,
    RB_BlendFactorKind_ConstantAlpha,
    RB_BlendFactorKind_OneMinusConstantAlpha,
    RB_BlendFactorKind_SrcAlphaSaturate,
    RB_BlendFactorKind_Src1Color,
    RB_BlendFactorKind_OneMinusSrc1Color,
    RB_BlendFactorKind_Src1Alpha,
    RB_BlendFactorKind_OneMinusSrc1Alpha,
}RB_BlendFactorKind;

typedef enum RB_BlendOpKind{
    RB_BlendOpKind_Add,
    RB_BlendOpKind_Subtract,
    RB_BlendOpKind_ReverseSubtract,
    RB_BlendOpKind_Min,
    RB_BlendOpKind_Max,
}RB_BlendOpKind;

typedef u8 RB_ColorWriteFlags;
enum{
    RB_ColorWriteBit_Red   = DOT_BIT(0),
    RB_ColorWriteBit_Green = DOT_BIT(1),
    RB_ColorWriteBit_Blue  = DOT_BIT(2),
    RB_ColorWriteBit_Alpha = DOT_BIT(3),
    RB_ColorWriteBit_All = RB_ColorWriteBit_Red
        | RB_ColorWriteBit_Green
        | RB_ColorWriteBit_Blue
        | RB_ColorWriteBit_Alpha
};

typedef struct RB_BlendState
{
    RB_BlendFactorKind src_color;
    RB_BlendFactorKind dst_color;
    RB_BlendOpKind     blend_op;

    RB_BlendFactorKind src_alpha;
    RB_BlendFactorKind dst_alpha;
    RB_BlendOpKind     alpha_blend_op;

    RB_ColorWriteFlags color_write_mask;
    b8 blend_enabled    : 1;
    b8 separate_blend   : 1;
}RB_BlendState;

typedef struct RB_ColorTargetDesc
{
    RB_FormatKind format;
    RB_BlendState blend;
}RB_ColorTargetDesc;

typedef struct RB_RasterDesc
{
    RB_RasterState       raster_state;

    RB_ColorTargetDesc   color_targets[RB_IMAGE_OUTPUTS_MAX];

    RB_FormatKind depth_format;   // Invalid = no depth attachment
    RB_FormatKind stencil_format; // Invalid = no stencil attachment
    String8 debug_name;
}RB_RasterDesc;

////////////////////////////////////////////////////////////////
/// RB_RENDER_PASS

typedef enum RB_LoadOpKind
{
    RB_LoadOpKind_Load,
    RB_LoadOpKind_Clear,
    RB_LoadOpKind_DontCare,
}RB_LoadOpKind;

typedef enum RB_StoreOpKind
{
    RB_StoreOpKind_Store,
    RB_StoreOpKind_DontCare,
}RB_StoreOpKind;

typedef struct RB_ColorAttachment
{
    RB_RenderView  view;
    RB_LoadOpKind  load_op;
    RB_StoreOpKind store_op;
    f32x4           clear_color;
}RB_ColorAttachment;

typedef struct RB_DepthAttachment
{
    RB_RenderView  view;
    RB_LoadOpKind  load_op;
    RB_StoreOpKind store_op;
    f32            clear_value;
}RB_DepthAttachment;

typedef struct RB_StencilAttachment
{
    RB_RenderView  view;
    RB_LoadOpKind  load_op;
    RB_StoreOpKind store_op;
    u32            clear_value;
}RB_StencilAttachment;

typedef struct RB_RenderingDesc
{
    i32x2 render_extent;
    RB_ColorAttachment   color_attachments[RB_IMAGE_OUTPUTS_MAX];
    RB_DepthAttachment   depth_attachment;
    RB_StencilAttachment stencil_attachment;
}RB_RenderingDesc;

typedef u32 RB_RenderingFlags;
enum
{
    RB_RenderingBit_None       = 0,
    RB_RenderingBit_Suspending = DOT_BIT(0),
    RB_RenderingBit_Resuming   = DOT_BIT(1),
};

typedef struct RB_Extent2D16
{
    i16 x, y;
    u16 w,h;
}RB_Extent2D16;

typedef struct RB_Viewport
{
    RB_Extent2D16 viewport_rect;
    f32 min_depth;
    f32 max_depth;
}RB_Viewport;

////////////////////////////////////////////////////////////////
/// RB_DRAW

typedef enum RB_IndexKind
{
    RB_IndexKind_U16,
    RB_IndexKind_U32,
}RB_IndexKind;

typedef struct RB_DrawIndirectArgs
{
    u32 vertex_count;
    u32 instance_count;
    u32 first_vertex;
    u32 first_instance;
}RB_DrawIndirectArgs;

typedef struct RB_DrawIndexedIndirectArgs
{
    u32 index_count;
    u32 instance_count;
    u32 first_index;
    i32 vertex_offset;
    u32 first_instance;
}RB_DrawIndexedIndirectArgs;

typedef struct RB_DispatchIndirectArgs
{
    u32 x, y, z;
}RB_DispatchIndirectArgs;
typedef RB_DispatchIndirectArgs RB_DrawMeshletsIndirectArgs;

////////////////////////////////////////////////////////////////
/// RB_BARRIER

typedef u32 RB_StageFlags;
enum
{
    RB_StageBit_None              = 0,
    RB_StageBit_Indirect          = DOT_BIT(0),
    RB_StageBit_IndexInput        = DOT_BIT(1),
    RB_StageBit_Vertex            = DOT_BIT(2),
    RB_StageBit_Task              = DOT_BIT(3),
    RB_StageBit_Mesh              = DOT_BIT(4),
    RB_StageBit_DepthStencilTests = DOT_BIT(5),
    RB_StageBit_Fragment          = DOT_BIT(6),
    RB_StageBit_ColorOutput       = DOT_BIT(7),
    RB_StageBit_Compute           = DOT_BIT(8),
    RB_StageBit_Transfer          = DOT_BIT(9),
    RB_StageBit_Host              = DOT_BIT(10),
    RB_StageBit_AllCommands       = DOT_BIT(11),
};

typedef u32 RB_AccessFlags;
enum
{
    RB_AccessBit_None              = 0,
    RB_AccessBit_TransferRead      = DOT_BIT(0),
    RB_AccessBit_TransferWrite     = DOT_BIT(1),
    RB_AccessBit_ShaderRead        = DOT_BIT(2),
    RB_AccessBit_ShaderWrite       = DOT_BIT(3),
    RB_AccessBit_ColorRead         = DOT_BIT(4),
    RB_AccessBit_ColorWrite        = DOT_BIT(5),
    RB_AccessBit_DepthStencilRead  = DOT_BIT(6),
    RB_AccessBit_DepthStencilWrite = DOT_BIT(7),
    RB_AccessBit_IndirectRead      = DOT_BIT(8),
    RB_AccessBit_IndexRead         = DOT_BIT(9),
    RB_AccessBit_HostRead          = DOT_BIT(10),
};

////////////////////////////////////////////////////////////////
/// RB_SUBMISSION

typedef struct RB_TimelinePoint
{
    RB_TimelineSemaphore semaphore;
    u64 value;
}RB_TimelinePoint;

typedef struct RB_SubmitDesc
{
    RB_CommandBuffer *command_buffers;
    u32 command_buffer_count;

    RB_TimelinePoint completion;
}RB_SubmitDesc;

////////////////////////////////////////////////////////////////
/// RB_SWAPCHAIN

ReflectEnum(u32, RB_PresentModeKind, 
{
    RB_PresentModeKind_Immediate,
    RB_PresentModeKind_Fifo,
    RB_PresentModeKind_FifoRelaxed,
    RB_PresentModeKind_Mailbox,
});

// RB_HANDLE_VALID(swapchain) false if minimized - nothing to render this tick
typedef struct RB_SwapchainFrame
{
    RB_RenderView view;
    i32x2 extent;
}RB_SwapchainFrame;

////////////////////////////////////////////////////////////////
/// RB_DEVICE

typedef enum RB_DeviceKind
{
    RB_DeviceKind_Null, // Headless
    RB_DeviceKind_Vk,
    RB_DeviceKind_Dx12,
    RB_DeviceKind_Auto = RB_DeviceKind_Vk,
    RB_DeviceKind_Count,
}RB_DeviceKind;

typedef struct RB_DeviceDesc
{
    u64                permanent_memory_size;
    u64                transient_memory_size;

    // bool             compute_queue;
    u32                timestamp_query_max; // Per command buffer; 0 disables rb_write_timestamp
    u32                frame_overlap;
    RB_FormatKind      swapchain_format;
    RB_FormatKind      depth_format;
    RB_PresentModeKind present_mode;
}RB_DeviceDesc;

typedef struct RB_DeviceInfo
{
    RB_Device handle;
    String8 name;
    b8 has_integrated_gpu;
    u32 vendor_id;
    u32 driver_version;
    u32 api_version;
    // u64 total_vram_bytes; // sum of device-local memory heaps; 0 on backends that don't track it

    u64 texture_heap_alignment;
    u64 texture_descriptor_size;
    u64 sampler_descriptor_size;
    RB_FormatKind swapchain_format;
    RB_FormatKind depth_format;
    f32 timestamp_period_ns;
    b8  mesh_shaders;
    b8  descriptor_heap;
}RB_DeviceInfo;

////////////////////////////////////////////////////////////////
/// RB_DEBUG_MESSAGE

typedef enum RB_DebugSeverity
{
    RB_DebugSeverity_Info,
    RB_DebugSeverity_Warning,
    RB_DebugSeverity_Error,
    RB_DebugSeverity_Count,
}RB_DebugSeverity;

typedef u32 RB_DebugCategoryFlags;
enum
{
    RB_DebugCategoryBit_General     = 1 << 0,
    RB_DebugCategoryBit_Validation  = 1 << 1,
    RB_DebugCategoryBit_Performance = 1 << 2,
};

typedef struct RB_DebugMessage
{
    RB_DeviceKind         device_kind;
    RB_DebugSeverity      severity;
    RB_DebugCategoryFlags categories;
    i64                   id;
    String8               id_name;
    String8               text;
    void                 *native;
}RB_DebugMessage;

typedef void RB_DebugMessageFn(RB_DebugMessage *msg, void *user_data);

internal void rb_debug_message_set(RB_DebugMessageFn *fn, void *user_data);
internal void rb_debug_message_emit(RB_DebugMessage *msg);
internal void rb_debug_message_default(RB_DebugMessage *msg, void *user_data);

////////////////////////////////////////////////////////////////
/// RB_PROCEDURES

#define RB_ProcedureXList \
X(init,                             void,                   (Arena *arena, PlatformNativeWindow window, RB_DeviceDesc const *desc))\
X(end,                              void,                   (void))\
X(device_info,                      RB_DeviceInfo,          (void))\
X(drawable_extent,                  i32x2,                  (void))\
X(drawable_extent_set,              void,                   (i32x2 extent))\
X(wait_idle,                        void,                   (void))\
X(timeline_create,                  RB_TimelineSemaphore,   (u64 initial_value))\
X(timeline_destroy,                 void,                   (RB_TimelineSemaphore sema))\
X(timeline_completed_value,         u64,                    (RB_TimelineSemaphore sema))\
X(timeline_wait,                    void,                   (RB_TimelinePoint point))\
X(command_pool_create,              RB_CommandPool,         (void))\
X(command_pool_destroy,             void,                   (RB_CommandPool pool))\
X(command_pool_reset,               void,                   (RB_CommandPool pool))\
X(commands_begin,                   RB_CommandBuffer,       (RB_CommandPool pool))\
X(gpu_heap_create,                  RB_GpuHeap,             (RB_MemoryKind kind, i64 bytes))\
X(gpu_heap_destroy,                 void,                   (RB_GpuHeap heap))\
X(gpu_heap_address,                 RB_GpuAddress,          (RB_GpuHeap heap))\
X(gpu_heap_write,                   void,                   (RB_GpuHeap heap, i64 offset, void *data, i64 size))\
X(texture_descriptor_write,         void,                   (RB_GpuHeap heap, u32 index, RB_Texture texture, RB_TextureDescriptorKind kind))\
X(sampler_descriptor_write,         void,                   (RB_GpuHeap heap, u32 index, RB_SamplerDesc *desc))\
X(texture_heap_create,              RB_TextureHeap,         (i64 bytes))\
X(texture_heap_destroy,             void,                   (RB_TextureHeap heap))\
X(texture_create,                   RB_Texture,             (RB_CommandBuffer cmd, RB_TextureDesc *desc, RB_TextureHeap heap, i64 offset))\
X(texture_destroy,                  void,                   (RB_Texture texture_handle))\
X(texture_size_align,               RB_SizeAlign,           (RB_TextureDesc *desc))\
X(render_view_create,               RB_RenderView,          (RB_Texture texture, RB_RenderViewDesc desc))\
X(render_view_destroy,              void,                   (RB_RenderView view))\
X(pipeline_graphics_create,         RB_Pipeline,            (String8 vs_spirv, String8 fs_spirv, RB_RasterDesc *raster))\
X(pipeline_mesh_create,             RB_Pipeline,            (String8 ts_spirv, String8 ms_spirv, String8 fs_spirv, RB_RasterDesc *raster))\
X(pipeline_compute_create,          RB_Pipeline,            (String8 cs_spirv))\
X(pipeline_destroy,                 void,                   (RB_Pipeline pipeline))\
X(swapchain_frame_acquire,          RB_SwapchainFrame,      (RB_CommandBuffer cmd))\
X(barrier,                          void,                   (RB_CommandBuffer cmd, RB_StageFlags before_stage, RB_AccessFlags before_access, RB_StageFlags after_stage, RB_AccessFlags after_access))\
X(copy_memory,                      void,                   (RB_CommandBuffer cmd, RB_GpuRange src, RB_GpuRange dst))\
X(copy_memory_to_texture,           void,                   (RB_CommandBuffer cmd, RB_GpuRange src, RB_Texture dst, RB_TextureCopyDesc copy))\
X(copy_texture_to_memory,           void,                   (RB_CommandBuffer cmd, RB_Texture src, RB_GpuRange dst, RB_TextureCopyDesc copy))\
X(write_timestamp,                  void,                   (RB_CommandBuffer cmd, RB_GpuAddress dst, RB_StageFlags stage))\
X(descriptor_heaps_set,             void,                   (RB_CommandBuffer cmd, RB_GpuHeap texture_heap, RB_GpuHeap sampler_heap))\
X(render_pass_begin,                void,                   (RB_CommandBuffer cmd, RB_RenderingDesc desc, RB_RenderingFlags flags))\
X(render_pass_end,                  void,                   (RB_CommandBuffer cmd))\
X(set_viewport,                     void,                   (RB_CommandBuffer cmd, RB_Viewport viewport))\
X(set_scissor,                      void,                   (RB_CommandBuffer cmd, RB_Extent2D16 scissor))\
X(set_depth_stencil_state,          void,                   (RB_CommandBuffer cmd, const RB_DepthStencilState *state))\
X(bind_pso,                         void,                   (RB_CommandBuffer cmd, RB_Pipeline pso))\
X(draw,                             void,                   (RB_CommandBuffer cmd, RB_GpuAddress root, u32 vertex_count, u32 instance_count, u32 first_vertex, u32 first_instance))\
X(draw_indexed,                     void,                   (RB_CommandBuffer cmd, RB_GpuAddress root, RB_GpuRange indices, RB_IndexKind index_kind, u32 index_count, u32 instance_count, u32 first_index, i32 vertex_offset, u32 first_instance))\
X(draw_indirect,                    void,                   (RB_CommandBuffer cmd, RB_GpuAddress root, RB_GpuRange args, u32 draw_count, u32 stride))\
X(draw_indexed_indirect,            void,                   (RB_CommandBuffer cmd, RB_GpuAddress root, RB_GpuRange indices, RB_IndexKind index_kind, RB_GpuRange args, u32 draw_count, u32 stride))\
X(draw_meshlets,                    void,                   (RB_CommandBuffer cmd, RB_GpuAddress root, u32 group_x, u32 group_y, u32 group_z))\
X(draw_meshlets_indirect,           void,                   (RB_CommandBuffer cmd, RB_GpuAddress root, RB_GpuRange args, u32 draw_count, u32 stride))\
X(dispatch,                         void,                   (RB_CommandBuffer cmd, RB_GpuAddress root, u32 group_x, u32 group_y, u32 group_z))\
X(dispatch_indirect,                void,                   (RB_CommandBuffer cmd, RB_GpuAddress root, RB_GpuRange args))\
X(submit,                           void,                   (RB_SubmitDesc *desc))\
X(submit_and_present,               void,                   (RB_SubmitDesc *desc))\

/*
    Points every rb_<name> at the chosen backend, then runs its init. Asserts if kind's backend
    isn't compiled in (RB_BACKEND_*). With RB_BACKEND_SINGLE, rb_<name> are const pointers fixed
    to the one backend (direct calls once optimized), so kind must be that backend: no Null switch
*/
internal void rb_begin(RB_DeviceKind kind, Arena *arena, PlatformNativeWindow window, RB_DeviceDesc const *desc);

#endif // !RB_DEVICE_H
