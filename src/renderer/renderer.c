DOT_SETTING_U32("Render", rn_gtransient_memory_size_b, DOT_MB(80));
DOT_SETTING_U32("Render", rn_gpermanent_memory_size_b, DOT_MB(80));
DOT_SETTING_U32("Render", rn_g_frame_arena_size_b, DOT_KB(32));
DOT_SETTING_U32("Render", rn_g_cleanup_resource_pool_max, 128);
DOT_SETTING_U32("Render", rn_g_texture_heap_size_b, DOT_MB(512));
DOT_SETTING_U32("Render", rn_g_data_heap_size_b, DOT_MB(64));
DOT_SETTING_U32("Render", rn_g_frame_heap_size_b, DOT_MB(4));   // per frame in flight
DOT_SETTING_U32("Render", rn_g_upload_heap_size_b, DOT_MB(32)); // per frame in flight
DOT_SETTING_U32("Render", rn_g_delete_queue_capacity, 256);
DOT_SETTING_U32("Render", rn_g_texture_descriptor_max, 16384);
DOT_SETTING_U32("Render", rn_g_sampler_descriptor_max, 32);

DOT_SETTING_U32("RenderDevice", rn_g_device_kind, RB_DeviceKind_Auto);
DOT_SETTING_U32("RenderDevice", rn_g_device_permanent_memory_size_b, DOT_MB(4));
DOT_SETTING_U32("RenderDevice", rn_g_device_transient_memory_size_b, DOT_KB(512));
DOT_SETTING_U32("RenderDevice", rn_g_timestamp_query_max, 64); // Per command buffer; 0 disables rb_write_timestamp
DOT_SETTING_U32("RenderDevice", rn_g_frame_overlap, 3);
DOT_SETTING_U32("RenderDevice", rn_g_swapchain_format, RB_FormatKind_RGBA8_SRGB);
DOT_SETTING_U32("RenderDevice", rn_g_depth_stencil_format, RB_FormatKind_D32_FLOAT);
DOT_SETTING_U32("RenderDevice", rn_g_present_mode, RB_PresentModeKind_Mailbox);

DOT_SETTING_U32("ShaderCache", rn_g_shader_cache_bucket_count, 64);
DOT_SETTING_U32("Shader", rn_g_shader_optimization_level, RN_ShaderOptimizationLevel_None);
DOT_SETTING_U32("Shader", rn_g_shader_debug_info_level, RN_ShaderDebugInfoLevel_Standard);
DOT_SETTING_U32("Shader", rn_g_shader_floating_point_mode, RN_ShaderFloatingPointMode_Default);
DOT_SETTING_BOOL("Shader", rn_g_shader_warnings_as_errors, false);

internal u32     rn_frame_slot(RN_RenderCtx *renderer);
internal void    rn_upload_texture(RN_RenderCtx *renderer, RB_Texture dst, void *src, i64 size, RB_TextureCopyDesc copy);
internal String8 rn_shader_spirv_get_or_compile(RN_RenderCtx *renderer, RB_ShaderStageKind stage_kind, String8 path);
internal void    rn_depth_target_resize(RN_RenderCtx *renderer);
internal void    rn_depth_target_destroy(RN_RenderCtx *renderer);
internal void    rn_resource_cleanup_list_push_texture(RN_RenderCtx *renderer, RB_Texture texture_id);
internal void    rn_resource_cleanup_list_remove_texture(RN_RenderCtx *renderer, RB_Texture texture_id);
internal void    rn_resource_cleanup_list_pop_all(RN_RenderCtx *renderer);

internal void
rn_init(Arena *arena, RN_RenderCtx *renderer, PlatformNativeWindow native_window)
{
    renderer->permanent_arena = ARENA_CREATE(
        .parent       = arena,
        .reserve_size = rn_gpermanent_memory_size_b,
        .name         = "Application render permanent");

    renderer->transient_arena = ARENA_CREATE(
        .parent       = arena,
        .reserve_size = rn_gtransient_memory_size_b,
        .name         = "Application renderer transient");

    TREE_INIT(renderer->permanent_arena, RN_ResourceCleanupCtx, &renderer->cleanup_tree, rn_g_cleanup_resource_pool_max);
    hm_init_string8(renderer->permanent_arena, &renderer->shader_cache, rn_g_shader_cache_bucket_count);

    renderer->frame_data_count = rn_g_frame_overlap;
    renderer->frame_datas    = PUSH_ARRAY(renderer->permanent_arena, RN_FrameData, renderer->frame_data_count);
    for(u8 i = 0; i < renderer->frame_data_count; ++i){
        renderer->frame_datas[i].temp_arena = ARENA_CREATE(
            .parent       = renderer->permanent_arena,
            .reserve_size = rn_g_frame_arena_size_b);
    }

    RB_DeviceDesc device_desc = {
        .permanent_memory_size = rn_g_device_permanent_memory_size_b,
        .transient_memory_size = rn_g_device_transient_memory_size_b,
        .timestamp_query_max   = rn_g_timestamp_query_max,
        .frame_overlap         = rn_g_frame_overlap,
        .swapchain_format      = cast(RB_FormatKind) rn_g_swapchain_format,
        .depth_format          = cast(RB_FormatKind) rn_g_depth_stencil_format,
        .present_mode          = cast(RB_PresentModeKind) rn_g_present_mode,
    };
    rb_begin(cast(RB_DeviceKind) rn_g_device_kind, renderer->permanent_arena, native_window, &device_desc);

    RB_DeviceInfo device_info = rb_device_info();
    DOT_PRINT("Render device: %S (%s)", device_info.name,device_info.has_integrated_gpu ? "integrated" : "discrete");
    renderer->swapchain_format = device_info.swapchain_format;
    renderer->depth_format     = device_info.depth_format;

    renderer->texture_heap = rb_texture_heap_arena_create(rn_g_texture_heap_size_b);
    renderer->data_heap    = rb_gpu_arena_create(rn_g_data_heap_size_b);
    for EACH_INDEX(i, renderer->frame_data_count){
        renderer->frame_datas[i].frame_heap  = rb_cpu_gpu_arena_create(rn_g_frame_heap_size_b);
        renderer->frame_datas[i].upload_heap = rb_cpu_gpu_arena_create(rn_g_upload_heap_size_b);
        renderer->frame_datas[i].frame_pool  = rb_command_pool_create();
        renderer->frame_datas[i].upload_pool = rb_command_pool_create();
    }
    mutex_init(&renderer->upload_mutex);
    renderer->upload_timeline = rb_timeline_create(0);
    renderer->frame_timeline  = rb_timeline_create(0);
    renderer->delete_queue    = rb_delete_queue_create(renderer->permanent_arena, renderer->frame_timeline, rn_g_delete_queue_capacity);
    renderer->texture_descriptor_heap = rb_gpu_heap_create(RB_MemoryKind_TextureDescriptorHeap, cast(i64)(rn_g_texture_descriptor_max * device_info.texture_descriptor_size));
    renderer->sampler_descriptor_heap = rb_gpu_heap_create(RB_MemoryKind_SamplerDescriptorHeap, cast(i64)(rn_g_sampler_descriptor_max * device_info.sampler_descriptor_size));
    renderer->sampler_slot_descs      = PUSH_ARRAY(renderer->permanent_arena, RB_SamplerDesc, rn_g_sampler_descriptor_max);

    RB_TextureDesc null_texture_desc = {0};
    null_texture_desc.width = 1;
    null_texture_desc.height = 1;
    null_texture_desc.depth = 1;
    null_texture_desc.array_layers = 1;
    null_texture_desc.mip_levels = 1;
    null_texture_desc.format_kind = RB_FormatKind_RGBA8_UNORM;
    null_texture_desc.dimension_kind = RB_TextureDimensionKind_2D;
    renderer->null_texture = rn_texture_create(renderer, &null_texture_desc , (u8[]){0,0,0,0}, NULL);
}

internal void
rn_shutdown(RN_RenderCtx *renderer)
{
    rb_wait_idle();
    rn_depth_target_destroy(renderer);
    rn_resource_cleanup_list_pop_all(renderer);
    rb_delete_queue_drain(&renderer->delete_queue);
    rb_texture_destroy(renderer->null_texture);

    for EACH_INDEX(i, renderer->frame_data_count){
        rb_cpu_gpu_arena_destroy(&renderer->frame_datas[i].frame_heap);
        rb_cpu_gpu_arena_destroy(&renderer->frame_datas[i].upload_heap);
        rb_command_pool_destroy(renderer->frame_datas[i].frame_pool);
        rb_command_pool_destroy(renderer->frame_datas[i].upload_pool);
    }
    rb_gpu_arena_destroy(&renderer->data_heap);
    rb_gpu_heap_destroy(renderer->texture_descriptor_heap);
    rb_gpu_heap_destroy(renderer->sampler_descriptor_heap);
    mutex_release(&renderer->upload_mutex);
    rb_texture_heap_arena_destroy(&renderer->texture_heap);
    rb_timeline_destroy(renderer->upload_timeline);
    rb_timeline_destroy(renderer->frame_timeline);
    rb_end();
}

// The next frame submit: the last one that can reference the resource.
internal u64
rn_retire_value(RN_RenderCtx *renderer)
{
    return(renderer->frame_value + 1);
}

void
rn_texture_destroy(RN_RenderCtx *renderer, RB_Texture handle)
{
    if(handle.h == renderer->null_texture.h){ return; } // failed loads fall back to it
    rn_resource_cleanup_list_remove_texture(renderer, handle);
    rb_delete_queue_defer_texture(&renderer->delete_queue, rn_retire_value(renderer), handle);
}

DOT_ENGINE_API void
rn_pipeline_destroy(RN_RenderCtx *renderer, RB_Pipeline handle)
{
    rb_delete_queue_defer_pipeline(&renderer->delete_queue, rn_retire_value(renderer), handle);
}

internal void
rn_depth_target_destroy(RN_RenderCtx *renderer)
{
    if(!RB_HANDLE_VALID(renderer->depth_view)){ return; }
    rb_delete_queue_defer_render_view(&renderer->delete_queue, rn_retire_value(renderer), renderer->depth_view);
    rb_delete_queue_defer_texture(&renderer->delete_queue, rn_retire_value(renderer), renderer->depth_texture);
    renderer->depth_view    = (RB_RenderView){0};
    renderer->depth_texture = (RB_Texture){0};
    renderer->depth_extent  = (i32x2){0};
}

internal void
rn_depth_target_resize(RN_RenderCtx *renderer)
{
    i32x2 extent = renderer->current_extent;
    if(RB_HANDLE_VALID(renderer->depth_view) && extent.x == renderer->depth_extent.x && extent.y == renderer->depth_extent.y){
        return;
    }
    rn_depth_target_destroy(renderer);

    RB_TextureDesc desc = {0};
    desc.dimension_kind = RB_TextureDimensionKind_2D;
    desc.format_kind    = renderer->depth_format;
    desc.width          = cast(u16) extent.x;
    desc.height         = cast(u16) extent.y;
    desc.depth          = 1;
    desc.array_layers   = 1;
    desc.mip_levels     = 1;
    desc.debug_name     = str_comp("Depth");

    renderer->depth_texture = rb_texture_create(renderer->current_cmd, &desc, (RB_TextureHeap){0}, 0);
    renderer->depth_view    = rb_render_view_create(renderer->depth_texture, (RB_RenderViewDesc){0});
    renderer->depth_extent  = extent;
}

internal u32
rn_frame_slot(RN_RenderCtx *renderer)
{
    return(cast(u32)(renderer->frame_value % renderer->frame_data_count));
}

internal RB_GpuRange
rn_frame_push_(RN_RenderCtx *renderer, void *data, i64 size)
{
    RB_GpuRange range = rb_cpu_gpu_arena_push_atomic_(&renderer->frame_datas[rn_frame_slot(renderer)].frame_heap, data, size);
    DOT_ASSERT(range.gpu, "Frame heap exhausted (%llu bytes requested)", size);
    return(range);
}

internal RB_GpuAddress
rn_data_alloc(RN_RenderCtx *renderer, i64 size)
{
    RB_GpuRange range = rb_gpu_arena_alloc_atomic(&renderer->data_heap, size);
    DOT_ASSERT(range.gpu, "Data heap exhausted (%llu bytes requested)", size);
    return(range.gpu);
}

internal void
rn_upload_begin(RN_RenderCtx *renderer)
{
    mutex_lock(&renderer->upload_mutex);
    renderer->upload_cmd = rb_commands_begin(renderer->frame_datas[rn_frame_slot(renderer)].upload_pool);
}

internal RB_TimelinePoint
rn_upload_submit_(RN_RenderCtx *renderer)
{
    rb_barrier(renderer->upload_cmd,
        RB_StageBit_Transfer,    RB_AccessBit_TransferWrite,
        RB_StageBit_AllCommands, RB_AccessBit_ShaderRead | RB_AccessBit_IndexRead | RB_AccessBit_IndirectRead);
    renderer->upload_value += 1;
    RB_TimelinePoint completion = { renderer->upload_timeline, renderer->upload_value };
    rb_submit(&(RB_SubmitDesc){ .command_buffers = &renderer->upload_cmd, .command_buffer_count = 1, .completion = completion });
    renderer->frame_datas[rn_frame_slot(renderer)].last_submit = completion;
    return(completion);
}

// A full staging heap flushes the batch so far, waits for it, and continues in a new command buffer.
internal RB_GpuRange
rn_upload_stage_(RN_RenderCtx *renderer, void *src, i64 size)
{
    RN_FrameData *frame_data = &renderer->frame_datas[rn_frame_slot(renderer)];
    RB_CpuGpuArena *heap = &frame_data->upload_heap;
    DOT_ASSERT(size <= heap->size, "Upload of %M bytes exceeds the upload heap", size);
    if(!rb_cpu_gpu_arena_fits(heap, size)){
        rb_timeline_wait(rn_upload_submit_(renderer));
        rb_cpu_gpu_arena_reset(heap);
        renderer->upload_cmd = rb_commands_begin(frame_data->upload_pool);
    }
    return(rb_cpu_gpu_arena_push_atomic_(heap, src, size));
}

internal void
rn_upload_memory(RN_RenderCtx *renderer, RB_GpuAddress dst, void *src, i64 size)
{
    RB_GpuRange staging = rn_upload_stage_(renderer, src, size);
    rb_copy_memory(renderer->upload_cmd, staging, (RB_GpuRange){ dst, size });
}

internal void
rn_upload_texture(RN_RenderCtx *renderer, RB_Texture dst, void *src, i64 size, RB_TextureCopyDesc copy)
{
    RB_GpuRange staging = rn_upload_stage_(renderer, src, size);
    rb_copy_memory_to_texture(renderer->upload_cmd, staging, dst, copy);
}

internal RB_TimelinePoint
rn_upload_end(RN_RenderCtx *renderer)
{
    RB_TimelinePoint completion = rn_upload_submit_(renderer);
    renderer->upload_cmd = (RB_CommandBuffer){0};
    mutex_unlock(&renderer->upload_mutex);
    return(completion);
}

internal RB_Texture
rn_texture_create(RN_RenderCtx *renderer, RB_TextureDesc *texture_desc, void *data, RB_Texture *fallback)
{
    RB_SizeAlign size_align = rb_texture_size_align(texture_desc);
    i64 offset = rb_texture_heap_arena_place(&renderer->texture_heap, size_align);

    rn_upload_begin(renderer);
    RB_Texture h = rb_texture_create(renderer->upload_cmd, texture_desc, renderer->texture_heap.heap, offset);
    if(RB_HANDLE_VALID(h)){
        rn_resource_cleanup_list_push_texture(renderer, h);
        if(data){
            const RB_TextureFormatInfo format_info = rb_texture_format_info_from_format(texture_desc->format_kind);
            u8 *src = cast(u8*)data;
            u32 mip_count = DOT_MAX(texture_desc->mip_levels, 1);
            for(u32 slice = 0; slice < texture_desc->array_layers; ++slice){
                for(u32 mip = 0; mip < mip_count; ++mip){
                    i64 mip_w = DOT_MAX(texture_desc->width  >> mip, 1);
                    i64 mip_h = DOT_MAX(texture_desc->height >> mip, 1);
                    i64 mip_d = DOT_MAX(texture_desc->depth  >> mip, 1);
                    i64 mip_bytes = format_info.block_size * mip_w * mip_h * mip_d;
                    rn_upload_texture(renderer, h, src, mip_bytes, (RB_TextureCopyDesc){ .slice = slice, .mip_level = mip });
                    src += mip_bytes;
                }
            }
        }
    }
    rn_upload_end(renderer);

    if(!RB_HANDLE_VALID(h)){
        DOT_ASSERT(fallback, "couldn't allocate texture and no fallback given");
        return(*fallback);
    }
    return(h);
}

// (jd) NOTE: Renderer shouldn't be doing the loading
internal f32
rn_mip_texel_read_(u8 *texel, u8 size_bytes)
{
    switch(size_bytes){
    case 1:  return(cast(f32) *texel);
    case 2:  return(cast(f32) *cast(u16*)texel);
    default: return(*cast(f32*)texel);
    }
}

internal void
rn_mip_texel_write_(u8 *texel, u8 size_bytes, f32 value)
{
    switch(size_bytes){
    case 1:  *texel           = cast(u8)(value + 0.5f);  break;
    case 2:  *cast(u16*)texel = cast(u16)(value + 0.5f); break;
    default: *cast(f32*)texel = value;                   break;
    }
}

internal void *
rn_mip_chain_build(Arena *arena, void *mip0, int width, int height, int comp, u8 size_bytes, u8 mip_levels)
{
    i64 texel_bytes = cast(i64) comp * size_bytes;
    i64 total = 0;
    for(u8 mip = 0; mip < mip_levels; ++mip){
        total += texel_bytes * DOT_MAX(width >> mip, 1) * DOT_MAX(height >> mip, 1);
    }
    u8 *chain = PUSH_ARRAY(arena, u8, cast(u64) total);
    MEMORY_COPY(chain, mip0, cast(u64)(texel_bytes * width * height));

    u8 *src = chain;
    int src_w = width;
    int src_h = height;
    for(u8 mip = 1; mip < mip_levels; ++mip){
        int dst_w = DOT_MAX(src_w / 2, 1);
        int dst_h = DOT_MAX(src_h / 2, 1);
        u8 *dst = src + texel_bytes * src_w * src_h;
        for(int y = 0; y < dst_h; ++y){
            int y0 = DOT_MIN(y * 2, src_h - 1);
            int y1 = DOT_MIN(y * 2 + 1, src_h - 1);
            for(int x = 0; x < dst_w; ++x){
                int x0 = DOT_MIN(x * 2, src_w - 1);
                int x1 = DOT_MIN(x * 2 + 1, src_w - 1);
                for(int c = 0; c < comp; ++c){
                    i64 component = cast(i64) c * size_bytes;
                    f32 sum = rn_mip_texel_read_(src + (cast(i64) y0 * src_w + x0) * texel_bytes + component, size_bytes)
                            + rn_mip_texel_read_(src + (cast(i64) y0 * src_w + x1) * texel_bytes + component, size_bytes)
                            + rn_mip_texel_read_(src + (cast(i64) y1 * src_w + x0) * texel_bytes + component, size_bytes)
                            + rn_mip_texel_read_(src + (cast(i64) y1 * src_w + x1) * texel_bytes + component, size_bytes);
                    rn_mip_texel_write_(dst + (cast(i64) y * dst_w + x) * texel_bytes + component, size_bytes, sum * 0.25f);
                }
            }
        }
        src   = dst;
        src_w = dst_w;
        src_h = dst_h;
    }
    return(chain);
}

RN_BindlessTexture
rn_texture_load_from_path(RN_RenderCtx *renderer, String8 name, String8 path, u8 mip_levels)
{
    DOT_PRINT("Creating texture with name %S, from path %S: ", name, path);
    TempArena temp = temp_arena_start(renderer->transient_arena);
    String8 file_data = platform_read_entire_file(temp.arena, path);

    void *data = NULL;
    u8 size_bytes = 0;
    b32 is_hdr = cast(b32)stbi_is_hdr_from_memory(file_data.str, cast(int)file_data.size);
    b32 is_16_bit = is_hdr || stbi_is_16_bit_from_memory(file_data.str, cast(int)file_data.size);
    int width = 0;
    int height = 0;
    int comp = 0;
    stbi_allocator alloc = {.stbi_alloc = arena_push, .stbi_free = arena_free, .arena = temp.arena};
    stbi_allocator_set(&alloc);
    stbi_info_from_memory(file_data.str, cast(int)file_data.size, &width, &height, &comp);
    comp = comp == 3 ? STBI_rgb_alpha : comp;
    if(is_hdr){
        data = stbi_loadf_from_memory(file_data.str, cast(int)file_data.size, &width, (int*)&height, NULL, comp);
        size_bytes = 4;
    }else if(is_16_bit){
        data = stbi_load_16_from_memory(file_data.str, cast(int)file_data.size, &width, &height, NULL, comp);
        size_bytes = 2;
    }else{
        data = stbi_load_from_memory(file_data.str, cast(int)file_data.size, &width, &height, NULL, comp);
        size_bytes = 1;
    }

    if(mip_levels == 0){
        int mip_w = width;
        int mip_h = height;
        mip_levels = 1;
        while(mip_w > 1 || mip_h > 1){
            mip_w = mip_w > 1 ? mip_w / 2 : 1;
            mip_h = mip_h > 1 ? mip_h / 2 : 1;
            mip_levels++;
        }
    }

    if(data && mip_levels > 1){
        data = rn_mip_chain_build(temp.arena, data, width, height, comp, size_bytes, mip_levels);
    }

    RB_TextureDesc desc = {0};
    desc.dimension_kind = RB_TextureDimensionKind_2D;
    desc.format_kind    = rb_texture_format_from_info(comp, size_bytes, true);
    desc.mip_levels     = mip_levels;
    desc.width          = cast(u16)width;
    desc.height         = cast(u16)height;
    desc.depth          = 1;
    desc.array_layers   = 1;
    desc.debug_name     = name;

    RN_BindlessTexture result = rn_texture_create_bindless(renderer, &desc, data);

    stbi_allocator_unset();
    temp_arena_restore(temp);
    return(result);
}

internal RN_BindlessTexture
rn_texture_create_bindless(RN_RenderCtx *renderer, RB_TextureDesc *desc, void *data)
{
    RN_BindlessTexture result = {0};
    result.handle = rn_texture_create(renderer, desc, data, &renderer->null_texture);

    DOT_ASSERT(renderer->next_texture_slot < rn_g_texture_descriptor_max, "Out of texture descriptor slots (%u)", rn_g_texture_descriptor_max);
    result.bindless_slot = renderer->next_texture_slot++;
    rb_texture_descriptor_write(renderer->texture_descriptor_heap, result.bindless_slot, result.handle, RB_TextureDescriptorKind_Sampled);
    return(result);
}

internal b32
rn_sampler_desc_match(RB_SamplerDesc *a, RB_SamplerDesc *b)
{
    b32 result = a->min_filter     == b->min_filter
              && a->mag_filter     == b->mag_filter
              && a->mipmap_filter  == b->mipmap_filter
              && a->address_mode_u == b->address_mode_u
              && a->address_mode_v == b->address_mode_v
              && a->address_mode_w == b->address_mode_w;
    return(result);
}

internal u32
rn_sampler_bindless_slot_create(RN_RenderCtx *renderer, RB_SamplerDesc *desc)
{
    for(u32 slot = 0; slot < renderer->next_sampler_slot; ++slot){
        if(rn_sampler_desc_match(&renderer->sampler_slot_descs[slot], desc)){
            return(slot);
        }
    }
    DOT_ASSERT(renderer->next_sampler_slot < rn_g_sampler_descriptor_max, "Out of sampler descriptor slots (%u)", rn_g_sampler_descriptor_max);
    u32 slot = renderer->next_sampler_slot++;
    renderer->sampler_slot_descs[slot] = *desc;
    rb_sampler_descriptor_write(renderer->sampler_descriptor_heap, slot, desc);
    return(slot);
}

DOT_ENGINE_API RB_Pipeline
rn_pipeline_create(RN_RenderCtx *renderer, String8 vs_path, String8 fs_path, RB_RasterDesc *raster)
{
    String8 vs_spirv = rn_shader_spirv_get_or_compile(renderer, RB_ShaderStageKind_Vertex, vs_path);
    String8 fs_spirv = rn_shader_spirv_get_or_compile(renderer, RB_ShaderStageKind_Fragment, fs_path);
    RB_Pipeline h = rb_pipeline_graphics_create(vs_spirv, fs_spirv, raster);
    return(h);
}


DOT_ENGINE_API RB_Pipeline
rn_graphics_pipeline_create(RN_RenderCtx *renderer, RN_PSOGraphics *graphics_pso)
{
    String8 vs_spirv = rn_shader_spirv_get_or_compile(renderer, RB_ShaderStageKind_Vertex, graphics_pso->vs_path);
    String8 fs_spirv = rn_shader_spirv_get_or_compile(renderer, RB_ShaderStageKind_Fragment, graphics_pso->fs_path);
    RB_Pipeline h = rb_pipeline_graphics_create(vs_spirv, fs_spirv, &graphics_pso->raster_desc);
    return(h);
}

void
rn_clear_background(RN_RenderCtx *renderer, f32x3 color)
{
    if(!renderer->current_frame_valid){ return; }

    RB_RenderingDesc desc = {
        .render_extent = renderer->current_extent,
        .color_attachments = { {
                .view = renderer->current_view,
                .load_op = RB_LoadOpKind_Clear,
                .store_op = RB_StoreOpKind_Store,
                .clear_color = v4(color.r, color.g, color.b, 1.0f),
        } }
    };

    rb_render_pass_begin(renderer->current_cmd, desc, RB_RenderingBit_None);
    rb_render_pass_end(renderer->current_cmd);
}

internal void
rn_frame_begin(RN_RenderCtx *renderer, i32x2 framebuffer_size)
{
    rb_delete_queue_tick(&renderer->delete_queue);

    // Under upload_mutex so no upload batch is open on this slot's pools/heaps while they reset.
    RN_FrameData *frame_data = &renderer->frame_datas[rn_frame_slot(renderer)];
    MUTEX_SCOPE(&renderer->upload_mutex){
        if(RB_HANDLE_VALID(frame_data->last_submit.semaphore)){
            rb_timeline_wait(frame_data->last_submit);
        }
        rb_command_pool_reset(frame_data->frame_pool);
        rb_command_pool_reset(frame_data->upload_pool);
        rb_cpu_gpu_arena_reset(&frame_data->frame_heap);
        rb_cpu_gpu_arena_reset(&frame_data->upload_heap);
    }
    renderer->current_cmd = rb_commands_begin(frame_data->frame_pool);
    rb_texture_descriptor_heap_set(renderer->current_cmd, renderer->texture_descriptor_heap);
    rb_sampler_descriptor_heap_set(renderer->current_cmd, renderer->sampler_descriptor_heap);
    rb_drawable_extent_set(framebuffer_size);

    RB_SwapchainFrame frame = rb_swapchain_frame_acquire(renderer->current_cmd);
    renderer->current_frame_valid = RB_HANDLE_VALID(frame.view);
    if(!renderer->current_frame_valid){
        return;
    }
    renderer->current_view   = frame.view;
    renderer->current_extent = frame.extent;
    rn_depth_target_resize(renderer);
}

internal void
rn_frame_end(RN_RenderCtx *renderer)
{
    if(!renderer->current_frame_valid){ return; }

    // Under upload_mutex so an upload batch never stages in one frame slot and submits in the next.
    MUTEX_SCOPE(&renderer->upload_mutex){
        RB_TimelinePoint completion = { renderer->frame_timeline, renderer->frame_value + 1 };
        rb_submit_and_present(&(RB_SubmitDesc){ .command_buffers = &renderer->current_cmd, .command_buffer_count = 1, .completion = completion });
        renderer->frame_datas[rn_frame_slot(renderer)].last_submit = completion;
        renderer->frame_value = completion.value;
    }
}

// NOTE(JD): Will start to just make enclosing scopes
// Should extend to be a graph
internal void
rn_resource_cleanup_list_push_texture(RN_RenderCtx *renderer, RB_Texture texture_id)
{
    DOT_UNUSED(renderer, texture_id);
    // RN_ResourceCleanupCtx *root = TREE_GET_ROOT(&renderer->cleanup_tree);
    // TODO: We still cannot clear individual resources since this would leave holes in the tree array
    // ARRAY_PUSH(root->texture_ids, texture_id);
}

internal void
rn_resource_cleanup_list_remove_texture(RN_RenderCtx *renderer, RB_Texture texture_id)
{
    RN_ResourceCleanupCtx *root = TREE_GET_ROOT(&renderer->cleanup_tree);
    for EACH_INDEX(i, root->texture_ids.count){
        if(ARRAY_GET(root->texture_ids, i).h == texture_id.h){
            root->texture_ids.count -= 1;
            ARRAY_GET(root->texture_ids, i) = ARRAY_GET(root->texture_ids, root->texture_ids.count);
            return;
        }
    }
}

internal void
rn_resource_cleanup_list_pop_last()
{
}

internal void
rn_resource_cleanup_list_pop_at(PoolHandle pop_start)
{
    (void)pop_start;
}

internal void
rn_resource_cleanup_list_pop_all(RN_RenderCtx *renderer)
{
    Scratch t = scratch_begin(0,0);
    RN_ResourceCleanupTree *tree = &renderer->cleanup_tree;
    TreeIterator it = TREE_ITER_BEGIN(t.arena, tree, tree->tree_pool.tree_root);
    for EACH_TREE_NODE(node_h, &it){
        RN_ResourceCleanupCtx *cleanup_list = TREE_GET(tree, node_h);
        for EACH_INDEX(i, cleanup_list->texture_ids.count){
            RB_Texture tex_h = ARRAY_GET(cleanup_list->texture_ids, i);
            rb_delete_queue_defer_texture(&renderer->delete_queue, rn_retire_value(renderer), tex_h);
        }
        cleanup_list->texture_ids.count = 0;
    }
    scratch_end(t);
}

internal String8
rn_shader_spirv_get_or_compile(RN_RenderCtx *renderer, RB_ShaderStageKind stage_kind, String8 path)
{
    DOT_ASSERT(stage_kind < RB_ShaderStageKind_Count);
    RN_ShaderCompileOptions options = {
        .optimization   = cast(RN_ShaderOptimizationLevel)rn_g_shader_optimization_level,
        .debug_info     = cast(RN_ShaderDebugInfoLevel)rn_g_shader_debug_info_level,
        .floating_point = cast(RN_ShaderFloatingPointMode)rn_g_shader_floating_point_mode,
        .flags          = rn_g_shader_warnings_as_errors ? RN_ShaderCompileFlag_WarningsAsErrors : 0,
    };
    String8 compiled_path = rn_shader_cache_get_compiled_path(renderer->permanent_arena, path, &options);

    b32 need_recompile = platform_file_is_newer(path, compiled_path);
    b32 compilation_success = false;
    if(need_recompile){
        DOT_PRINT("Recompiling %S", path);
        compilation_success = rn_shader_compile_from_path(stage_kind, &options, path, compiled_path);
        if(!compilation_success){
            DOT_WARNING("Failed to compile shader %S", path);
        }
    }

    RN_ShaderCachedData *shader_stage = hm_get_or_create(renderer->permanent_arena, &renderer->shader_cache, path);

    b32 just_created = !rn_shader_module_is_initialized(shader_stage);
    b32 should_update_shader = (need_recompile && compilation_success) || just_created;
    if(should_update_shader){
        shader_stage->compiled_path = compiled_path;
        shader_stage->path = path;
        shader_stage->byte_code = platform_read_entire_file(renderer->permanent_arena, compiled_path);
        if(shader_stage->byte_code.size == 0){
            DOT_WARNING("Failed to read shader %S compilation %S", path, compiled_path);
        }
    }
    return(shader_stage->byte_code);
}
