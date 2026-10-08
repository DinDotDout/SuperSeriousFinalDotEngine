#ifndef RB_BACKEND_NULL_H
#define RB_BACKEND_NULL_H

DOT_DIAGNOSTIC_PUSH
#if DOT_COMPILER_CLANG || DOT_COMPILER_GCC
    DOT_DIAGNOSTIC_IGNORE("-Wunused-parameter")
#elif DOT_COMPILER_MSVC
    DOT_DIAGNOSTIC_IGNORE(4100)
#endif

#define X(name, ret, params) internal ret rb_null_##name params;
RB_ProcedureXList
#undef X

internal void                   rb_null_instance_create(Arena *arena){}
internal void                   rb_null_init(Arena* a, PlatformNativeWindow window, RB_DeviceDesc const *desc){}
internal void                   rb_null_end(){}
internal void                   rb_null_shutdown(void){} internal RB_DeviceInfo rb_null_device_info(void){ RB_DeviceInfo d = {0}; return(d); }
internal b32                    rb_null_supports_texture_format(RB_FormatKind format, RB_TextureUsageFlags usage){ return(false); }
internal i32x2                  rb_null_drawable_extent(void){ return((i32x2){0}); }
internal void                   rb_null_drawable_extent_set(i32x2 extent){}
internal void                   rb_null_wait_idle(void){}
internal RB_TimelineSemaphore   rb_null_timeline_create(u64 initial_value){ return((RB_TimelineSemaphore){0}); }
internal void                   rb_null_timeline_destroy(RB_TimelineSemaphore sema){}
internal u64                    rb_null_timeline_completed_value(RB_TimelineSemaphore sema){ return(0); }
internal void                   rb_null_timeline_wait(RB_TimelinePoint point){}
internal RB_SwapchainFrame      rb_null_swapchain_frame_acquire(RB_CommandBuffer cmd){ return((RB_SwapchainFrame){0}); }
internal RB_CommandPool         rb_null_command_pool_create(void){ return((RB_CommandPool){0}); }
internal void                   rb_null_command_pool_destroy(RB_CommandPool pool){}
internal void                   rb_null_command_pool_reset(RB_CommandPool pool){}
internal RB_CommandBuffer       rb_null_commands_begin(RB_CommandPool pool){ return((RB_CommandBuffer){0}); }
internal void                   rb_null_submit(RB_SubmitDesc *desc){}
internal void                   rb_null_submit_and_present(RB_SubmitDesc *desc){}
internal void                   rb_null_render_pass_begin(RB_CommandBuffer cmd, RB_RenderingDesc desc, RB_RenderingFlags flags){}
internal void                   rb_null_render_pass_end(RB_CommandBuffer cmd){}
internal void                   rb_null_set_viewport(RB_CommandBuffer cmd, RB_Viewport viewport){}
internal void                   rb_null_set_scissor(RB_CommandBuffer cmd, RB_Extent2D16 scissor){}
internal void                   rb_null_set_depth_stencil_state(RB_CommandBuffer cmd, RB_DepthStencilState state){}
internal void                   rb_null_barrier(RB_CommandBuffer cmd, RB_StageFlags before_stage, RB_AccessFlags before_access, RB_StageFlags after_stage, RB_AccessFlags after_access){}
internal void                   rb_null_gpu_heap_di32x2estroy(RB_GpuHeap heap){}
internal RB_GpuAddress          rb_null_gpu_heap_address(RB_GpuHeap heap){ return(0); }
internal void                   rb_null_gpu_heap_write(RB_GpuHeap heap, i64 offset, void *data, i64 size){}
internal void                   rb_null_resource_cleanup_list_push(void){}
internal void                   rb_null_resource_cleanup_list_pop_at(PoolHandle idx){}
internal void                   rb_null_resource_cleanup_list_pop_last(void){}
internal void                   rb_null_copy_memory(RB_CommandBuffer cmd, RB_GpuRange src, RB_GpuRange dst){}
internal void                   rb_null_copy_memory_to_texture(RB_CommandBuffer cmd, RB_GpuRange src, RB_Texture dst, RB_TextureCopyDesc copy){}
internal void                   rb_null_copy_texture_to_memory(RB_CommandBuffer cmd, RB_Texture src, RB_GpuRange dst, RB_TextureCopyDesc copy){}
internal void                   rb_null_write_timestamp(RB_CommandBuffer cmd, RB_GpuAddress dst, RB_StageFlags stage){}
internal RB_SizeAlign           rb_null_texture_size_align(RB_TextureDesc *desc){ return((RB_SizeAlign){0}); }

// (jd) Empty handles
internal RB_TextureHeap         rb_null_texture_heap_create(i64 bytes){ return((RB_TextureHeap){0}); }
internal void                   rb_null_texture_heap_destroy(RB_TextureHeap heap){}

internal RB_Texture             rb_null_texture_create(RB_CommandBuffer cmd, RB_TextureDesc *desc, RB_TextureHeap heap, i64 offset) { return((RB_Texture){0}); }
internal void                   rb_null_texture_destroy(RB_Texture h){}

internal RB_RenderView          rb_null_render_view_create(RB_Texture texture, RB_RenderViewDesc desc){ return((RB_RenderView){0}); }
internal void                   rb_null_render_view_destroy(RB_RenderView view){}
internal RB_Pipeline            rb_null_pipeline_mesh_create(String8 ts_spirv, String8 ms_spirv, String8 fs_spirv, RB_RasterDesc *raster){ return((RB_Pipeline){0}); }

internal RB_Pipeline            rb_null_pipeline_compute_create(String8 cs_spirv){ return((RB_Pipeline){0}); }
internal RB_Pipeline            rb_null_pipeline_graphics_create(String8 vs_spirv, String8 fs_spirv, RB_RasterDesc *raster) { return((RB_Pipeline){0}); }
internal void                   rb_null_pipeline_destroy(RB_Pipeline pipeline){}

internal RB_GpuHeap             rb_null_gpu_heap_create(RB_MemoryKind kind, i64 bytes){ RB_GpuHeap h = {0}; return(h); }
internal void                   rb_null_gpu_heap_destroy(RB_GpuHeap h){}

internal void                   rb_null_texture_descriptor_write(RB_GpuHeap heap, u32 index, RB_Texture texture, RB_TextureDescriptorKind kind){}
internal void                   rb_null_sampler_descriptor_write(RB_GpuHeap heap, u32 index, RB_SamplerDesc *desc){}
internal void                   rb_null_texture_descriptor_heap_set(RB_CommandBuffer cmd, RB_GpuHeap heap){}
internal void                   rb_null_sampler_descriptor_heap_set(RB_CommandBuffer cmd, RB_GpuHeap heap){}
internal void                   rb_null_bind_pso(RB_CommandBuffer cmd, RB_Pipeline pso){}
internal void                   rb_null_draw(RB_CommandBuffer cmd, RB_GpuAddress root, u32 vertex_count, u32 instance_count, u32 first_vertex, u32 first_instance){}
internal void                   rb_null_draw_indexed(RB_CommandBuffer cmd, RB_GpuAddress root, RB_GpuRange indices, RB_IndexKind index_kind, u32 index_count, u32 instance_count, u32 first_index, i32 vertex_offset, u32 first_instance){}
internal void                   rb_null_draw_indirect(RB_CommandBuffer cmd, RB_GpuAddress root, RB_GpuRange args, u32 draw_count, u32 stride){}
internal void                   rb_null_draw_indexed_indirect(RB_CommandBuffer cmd, RB_GpuAddress root, RB_GpuRange indices, RB_IndexKind index_kind, RB_GpuRange args, u32 draw_count, u32 stride){}
internal void                   rb_null_draw_meshlets(RB_CommandBuffer cmd, RB_GpuAddress root, u32 group_x, u32 group_y, u32 group_z){}
internal void                   rb_null_draw_meshlets_indirect(RB_CommandBuffer cmd, RB_GpuAddress root, RB_GpuRange args, u32 draw_count, u32 stride){}

internal void                   rb_null_dispatch(RB_CommandBuffer cmd, RB_GpuAddress root, u32 group_x, u32 group_y, u32 group_z){}
internal void                   rb_null_dispatch_indirect(RB_CommandBuffer cmd, RB_GpuAddress root, RB_GpuRange args){}

DOT_DIAGNOSTIC_POP
#endif // !RB_BACKEND_NULL_H
