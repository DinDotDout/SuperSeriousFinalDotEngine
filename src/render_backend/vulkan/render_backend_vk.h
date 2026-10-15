#ifndef RB_VK_H
#define RB_VK_H

#include <vulkan/vulkan_core.h>

////////////////////////////////////////////////////////////////
/// RB_VK_HANDLES

typedef VkDeviceMemory RB_VK_TextureHeap ;
typedef VkSemaphore RB_VK_TimelineSemaphore;

////////////////////////////////////////////////////////////////
/// RB_VK_DEVICE

typedef struct RB_VK_Device{
    VkDevice         vk_device;
    VkPhysicalDevice vk_gpu;
    b32              is_integrated_gpu;

    VkPhysicalDeviceProperties *device_properties;

    u32     graphics_queue_idx;
    VkQueue graphics_queue;

    u32     present_queue_idx;
    VkQueue present_queue;

    // (jd) Resolved Settings
    f32 timestamp_period_ns;
    b8  mesh_shaders;
    b8  memory_budget;
    u32 timestamp_query_count; // 0 = timestamps off
    u32 texture_memory_type;
    u64 texture_heap_alignment;
}RB_VK_Device;

////////////////////////////////////////////////////////////////
/// RB_VK_MEMORY

typedef struct RB_VK_GpuHeap RB_VK_GpuHeap;
struct RB_VK_GpuHeap{
    RB_VK_GpuHeap  *next;

    VkDeviceMemory  vk_memory;
    i64             alloc_size;
    i64             size;
    RB_MemoryKind   kind;

    // (jd) Unused if GPU only memory
    void           *cpu_base;
    VkDeviceAddress gpu_base;
    VkBuffer        vk_buffer; // We need to keep this around to obtain VkDeviceAddress
};

////////////////////////////////////////////////////////////////
/// RB_VK_TEXTURE

typedef struct RB_VK_Texture RB_VK_Texture;
struct RB_VK_Texture
{
    RB_VK_Texture *next;
    VkImage     vk_image;
    VkImageView vk_image_view;
    VkExtent3D  vk_extent3d;
    VkImageViewType vk_view_type;

    VkFormat           vk_format;
    VkImageAspectFlags vk_aspect;
    VkImageUsageFlags  vk_usage;

    // (jd) This is only filled in when requires / prefers dedicated alloc is true
    VkDeviceMemory vk_memory;

    u8 mip_levels;
    u16 array_layers;

    DOT_DebugName16(debug_name);
};

////////////////////////////////////////////////////////////////
/// RB_VK_PIPELINE

typedef struct RB_VK_Pipeline RB_VK_Pipeline;
struct RB_VK_Pipeline{
    RB_VK_Pipeline      *next;
    VkPipeline           vk_pipeline;
    VkPipelineBindPoint  vk_bind_point;
};

////////////////////////////////////////////////////////////////
/// RB_VK_RENDER_PASS

typedef struct RB_VK_SwapchainImage RB_VK_SwapchainImage;
typedef struct RB_VK_RenderView RB_VK_RenderView;
struct RB_VK_RenderView{
    RB_VK_RenderView  *next; // free list; only meaningful once destroyed
    VkImage            vk_image;
    VkImageView        vk_image_view;
    VkFormat           vk_format;
    VkImageAspectFlags vk_aspect;
    VkExtent2D         extent;
    RB_VK_SwapchainImage *swapchain_image; // NULL for texture views
};

////////////////////////////////////////////////////////////////
/// RB_VK_COMMANDS

typedef struct RB_VK_CommandBuffer RB_VK_CommandBuffer;
struct RB_VK_CommandBuffer{
    RB_VK_CommandBuffer *next;
    VkCommandBuffer      vk_cmd;

    VkQueryPool    timestamp_pool;
    RB_GpuAddress *timestamp_dsts;
    u32            timestamp_count;

    u32 render_pass_count;
    b8  recorded_outside_pass;
    b8  begins_resuming;
    b8  ends_suspending;
    b8  in_render_pass;
    b8  pass_suspending;
    b8  holds_swapchain;

    u32 active_color_attachment_count;
    RB_VK_RenderView *active_color_attachments[RB_IMAGE_OUTPUTS_MAX];
    RB_VK_RenderView *active_depth_attachment;
    RB_VK_RenderView *active_stencil_attachment;
};

typedef struct RB_VK_CommandPool RB_VK_CommandPool;
struct RB_VK_CommandPool{
    RB_VK_CommandPool   *next;
    VkCommandPool        vk_pool;
    RB_VK_CommandBuffer *first;
    RB_VK_CommandBuffer *last;
    RB_VK_CommandBuffer *next_free;
};

////////////////////////////////////////////////////////////////
/// RB_VK_SWAPCHAIN

typedef struct RB_VK_SwapchainDetails{
    // IN
    i32x2 frame_buffer_size;
    VkFormat preferred_format;
    VkPresentModeKHR preferred_present_mode;

    VkSurfaceFormatKHR            best_surface_format;
    VkPresentModeKHR              best_present_mode;
    VkExtent2D                    surface_extent;
    VkSurfaceTransformFlagBitsKHR current_transform;
    u32                           image_count;
}RB_VK_SwapchainDetails;

struct RB_VK_SwapchainImage
{
    VkImage       vk_image;
    VkImageView   vk_image_view;
    VkImageLayout vk_layout; // GENERAL once acquired, PRESENT_SRC at present
};

typedef struct RB_VK_PresentContext
{
    VkSemaphore acquired;
    VkSemaphore rendered;
    VkFence     presented;
}RB_VK_PresentContext;

typedef struct RB_VK_Swapchain{
    VkSwapchainKHR swapchain;
    VkExtent2D     extent;
    VkFormat       image_format;

    u32 swapchain_image_count;
    RB_VK_SwapchainImage *swapchain_images;
    RB_VK_RenderView     *render_views;

    u32                   image_index;
    b8                    acquired;
    b8                    recreate_required;
    RB_VK_PresentContext *present_context;
    RB_VK_CommandBuffer  *transition_commands;
}RB_VK_Swapchain;

////////////////////////////////////////////////////////////////
/// RB_VK_CONTEXT

typedef struct RB_VK_BackendCtx{
    Arena            *permanent_arena;
    Arena            *transient_arena;

    PlatformNativeWindow native_window;
    RB_DeviceDesc    desc;

    RB_VK_Device     device;
    VkFormat         depth_format;

    RB_VK_Swapchain  swapchain;

    Arena            *swapchain_arena; // (jd) NOTE: Reset on recreation
    VkInstance      instance;
    VkSurfaceKHR    surface;

    u32                   present_context_count;
    u32                   next_present_context;
    RB_VK_PresentContext *present_contexts;

    VkPhysicalDeviceDescriptorHeapPropertiesEXT heap_properties;

    i32x2          drawable_extent_requested;

    RB_VK_GpuHeap *head;
    RB_VK_RenderView  *render_view_free;
    RB_VK_CommandPool *command_pool_free;
    RB_VK_Texture     *texture_free;
    RB_VK_Pipeline    *pipeline_free;

    // (jd) Debug
    VkDebugUtilsMessengerEXT debug_messenger;

    // (jd) Use later on once we have a TLSF?
    // NOTE: vk expects a malloc like allocator, which I don't intend on make or using for now
    // so our push arenas do not work for this :(
    VkAllocationCallbacks    vk_allocator;
}RB_VK_BackendCtx;

////////////////////////////////////////////////////////////////
/// RB_VK_PROCEDURES

#define X(name, ret, params) internal ret rb_vk_##name params;
RB_ProcedureXList
#undef X

#endif // !RB_VK_H
