internal RN_TextureFormatInfo
rn_texture_format_info_from_format(RN_TextureFormatKind fmt)
{
    RN_TextureFormatInfo info = {0};
    switch(fmt){
    case RN_TextureFormatKind_Invalid:
        return info;
    // 8‑bit formats
    case RN_TextureFormatKind_R8_UNORM:
    case RN_TextureFormatKind_R8_UINT:
        info.channels     = 1;
        info.block_size   = 1;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case RN_TextureFormatKind_RG8_UNORM:
        info.channels     = 2;
        info.block_size   = 2;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case RN_TextureFormatKind_RGB8_SRGB:
        info.format_flags |= RN_TextureFormatBit_SRGB;
        DOT_FALLTHROUGH;
    case RN_TextureFormatKind_RGB8_UNORM:
        info.channels     = 3;
        info.block_size   = 3;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case RN_TextureFormatKind_RGBA8_SRGB:
        info.format_flags |= RN_TextureFormatBit_SRGB;
        DOT_FALLTHROUGH;
    case RN_TextureFormatKind_RGBA8_UNORM:
        info.channels     = 4;
        info.block_size   = 4;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case RN_TextureFormatKind_BGRA8_SRGB:
        info.format_flags |= RN_TextureFormatBit_SRGB;
        DOT_FALLTHROUGH;
    case RN_TextureFormatKind_BGRA8_UNORM:
        info.channels     = 4;
        info.block_size   = 4;
        info.block_width  = 1;
        info.block_height = 1;
    break;

    // HDR capable formats
    case RN_TextureFormatKind_R16F:
        info.channels     = 1;
        info.block_size   = 2;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case RN_TextureFormatKind_RG16F:
        info.channels     = 2;
        info.block_size   = 4;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case RN_TextureFormatKind_RGBA16F:
        info.channels     = 4;
        info.block_size   = 8;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case RN_TextureFormatKind_R32F:
        info.channels     = 1;
        info.block_size   = 4;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case RN_TextureFormatKind_RG32F:
        info.channels     = 2;
        info.block_size   = 8;
        info.block_width  = 1;
        info.block_height = 1;
    break;
    case RN_TextureFormatKind_RGBA32F:
        info.channels     = 4;
        info.block_size   = 16;
        info.block_width  = 1;
        info.block_height = 1;
    break;

    // Depth / Stencil formats
    case RN_TextureFormatKind_D16_UNORM:
        info.channels     = 1;
        info.block_size   = 2;
        info.block_width  = 1;
        info.block_height = 1;
        info.format_flags |= RN_TextureFormatBit_Depth;
    break;
    case RN_TextureFormatKind_D24_UNORM_S8_UINT:
        info.channels     = 2;
        info.block_size   = 4;
        info.block_width  = 1;
        info.block_height = 1;
        info.format_flags |= RN_TextureFormatBit_Depth | RN_TextureFormatBit_Stencil;
    break;
    case RN_TextureFormatKind_D32_SFLOAT:
        info.channels     = 1;
        info.block_size   = 4;
        info.block_width  = 1;
        info.block_height = 1;
        info.format_flags |= RN_TextureFormatBit_Depth;
    break;
    case RN_TextureFormatKind_D32F_S8_UINT:
        info.channels     = 2;
        info.block_size   = 5; /* 32F + 8 stencil */
        info.block_width  = 1;
        info.block_height = 1;
        info.format_flags |= RN_TextureFormatBit_Depth | RN_TextureFormatBit_Stencil;
    break;
    // BC block‑compressed formats
    case RN_TextureFormatKind_BC1_SRGB:
        info.format_flags |= RN_TextureFormatBit_SRGB;
        DOT_FALLTHROUGH;
    case RN_TextureFormatKind_BC1:
        info.channels     = 4;
        info.block_size   = 8;
        info.block_width  = 4;
        info.block_height = 4;
        info.format_flags |= RN_TextureFormatBit_Compressed;
    break;
    case RN_TextureFormatKind_BC3_SRGB:
        info.format_flags |= RN_TextureFormatBit_SRGB;
        DOT_FALLTHROUGH;
    case RN_TextureFormatKind_BC3:
        info.channels     = 4;
        info.block_size   = 16;
        info.block_width  = 4;
        info.block_height = 4;
        info.format_flags |= RN_TextureFormatBit_Compressed;
    break;
    case RN_TextureFormatKind_BC7_SRGB:
        info.format_flags |= RN_TextureFormatBit_SRGB;
        DOT_FALLTHROUGH;
    case RN_TextureFormatKind_BC7:
        info.channels     = 4;
        info.block_size   = 16;
        info.block_width  = 4;
        info.block_height = 4;
        info.format_flags |= RN_TextureFormatBit_Compressed;
    break;

    // ETC2 formats
    case RN_TextureFormatKind_ETC2_RGB8:
        info.channels     = 3;
        info.block_size   = 8;
        info.block_width  = 4;
        info.block_height = 4;
        info.format_flags |= RN_TextureFormatBit_Compressed;
    break;
    case RN_TextureFormatKind_ETC2_RGBA8:
        info.channels     = 4;
        info.block_size   = 16;
        info.block_width  = 4;
        info.block_height = 4;
        info.format_flags |= RN_TextureFormatBit_Compressed;
    break;
    }
    return info;
}

// (JD) NOTE: ideally we would pass in something like RN_TextureFormatInfo in the future
internal RN_TextureFormatKind
rn_texture_format_from_info(int comp, u8 size_bytes, b32 srgb)
{
    if(size_bytes == 4){ // 32-bit float per channel (.hdr) 
        switch(comp){
        case 1: return RN_TextureFormatKind_R32F;
        case 2: return RN_TextureFormatKind_RG32F;
        case 3: return RN_TextureFormatKind_RGBA32F;
        case 4: return RN_TextureFormatKind_RGBA32F;
        }
    }else if(size_bytes == 2){ // 16-bit integer formats (PNG, TGA, etc.)
        switch(comp){
        case 1: return RN_TextureFormatKind_R16F;
        case 2: return RN_TextureFormatKind_RG16F;
        case 3: return RN_TextureFormatKind_RGBA16F;
        case 4: return RN_TextureFormatKind_RGBA16F;
        }
    }else if(size_bytes == 1){
        switch(comp){ // 8-bit formats
        case 1: return RN_TextureFormatKind_R8_UNORM;
        case 2: return RN_TextureFormatKind_RG8_UNORM;
        case 3: return srgb ? RN_TextureFormatKind_RGB8_SRGB : RN_TextureFormatKind_RGB8_UNORM;
        case 4: return srgb ? RN_TextureFormatKind_RGBA8_SRGB : RN_TextureFormatKind_RGBA8_UNORM;
        }
    }
    return RN_TextureFormatKind_Invalid;
}

// ////////////////////////////////////////////////////////////////
// RN_PipelineDesc
//

internal RN_ShaderResourceLayoutDesc
rn_shader_resource_layout_begin()
{
    return (RN_ShaderResourceLayoutDesc){0};
}

internal void
rn_pipeline_set_vertex_attributes_(RN_PipelineDesc *pl, u32 count, RN_VertexAttribute *attr)
{
    DOT_ASSERT(count < DOT_ARRAY_COUNT(pl->vertex_input.vertex_attributes));
    pl->vertex_input.vertex_attribute_count = count;
    for (u32 i = 0; i < count; ++i) {
        pl->vertex_input.vertex_attributes[i] = attr[i];
    }
}

internal void
rn_pipeline_set_vertex_streams_(RN_PipelineDesc *pl, u32 count, RN_VertexStream *streams)
{
    DOT_ASSERT(count < DOT_ARRAY_COUNT(pl->vertex_input.vertex_streams));
    pl->vertex_input.vertex_stream_count = count;
    for (u32 i = 0; i < count; ++i) {
        pl->vertex_input.vertex_streams[i] = streams[i];
    }
}

internal void
rn_pipeline_set_depth_stencil_state_(RN_PipelineDesc *pl, RN_DepthStencilState *depth_stencil)
{
    pl->depth_stencil_state = *depth_stencil;
}

internal void
rn_pipeline_set_shader_state_(RN_PipelineDesc *pl, u32 count, RN_ShaderStage *stages)
{
    DOT_ASSERT(count < DOT_ARRAY_COUNT(pl->shader_state.shader_stages));
    pl->shader_state.shader_stage_count = count;
    for (u32 i = 0; i < count; ++i) {
        pl->shader_state.shader_stages[i] = stages[i];
    }
}

internal void
rn_pipeline_set_render_pass_output(RN_PipelineDesc *pl, RN_RenderPassOutput render_pass_output)
{
    pl->render_pass = render_pass_output;
}

internal void
rn_pipeline_push_shader_resource_layout(RN_PipelineDesc *pipeline_desc, RN_ShaderResourceLayoutHandle h){
    DOT_ASSERT(pipeline_desc->shader_resource_layout_count < DOT_ARRAY_COUNT(pipeline_desc->shader_resource_layouts));
    pipeline_desc->shader_resource_layouts[pipeline_desc->shader_resource_layout_count++] = h;
}

///////////////////////////////////////////////////////////////////
//
// RN_ShaderResourceLayoutDesc
//

internal void
rn_shader_resource_layout_set_bindings_(RN_ShaderResourceLayoutDesc *layout_desc, u16 binding_count, RN_ShaderResourceBinding *bindings)
{
    DOT_ASSERT(binding_count < DOT_ARRAY_COUNT(layout_desc->bindings));
    layout_desc->binding_count = binding_count;
    for(u32 i = 0; i < binding_count; ++i){
        layout_desc->bindings[i] = bindings[i];
    }
}

internal void
rn_shader_resource_layout_push_binding_(RN_ShaderResourceLayoutDesc *layout_desc, RN_ShaderResourceBinding *binding)
{
    DOT_ASSERT(layout_desc->binding_count < DOT_ARRAY_COUNT(layout_desc->bindings));
    layout_desc->bindings[layout_desc->binding_count++] = *binding;
}

///////////////////////////////////////////////////////////////////
// RN_ShaderStageKind
//


// (jd) didn't really need to do the hash thing but I wanted to test this for where it may matter
RN_ShaderStageKind
rn_shader_stage_kind_from_ext(String8 ext)
{
    String8 match = {0};
    RN_ShaderStageKind shader_kind = RN_ShaderStageKind_None;
    u32 h = fnv1a_runtime(ext);
    switch(h){
    case DOT_FNV1A_4(RN_EXT_VERT):  match = string8_lit(RN_EXT_VERT);  shader_kind = RN_ShaderStageKind_Vertex; break;
    case DOT_FNV1A_4(RN_EXT_FRAG):  match = string8_lit(RN_EXT_FRAG);  shader_kind = RN_ShaderStageKind_Fragment; break;
    case DOT_FNV1A_4(RN_EXT_COMP):  match = string8_lit(RN_EXT_COMP);  shader_kind = RN_ShaderStageKind_Compute; break;

    case DOT_FNV1A_4(RN_EXT_RGEN):  match = string8_lit(RN_EXT_RGEN);  shader_kind = RN_ShaderStageKind_RayGen; break;
    case DOT_FNV1A_5(RN_EXT_RAHIT): match = string8_lit(RN_EXT_RAHIT); shader_kind = RN_ShaderStageKind_RayHitAny; break;
    case DOT_FNV1A_5(RN_EXT_RCHIT): match = string8_lit(RN_EXT_RCHIT); shader_kind = RN_ShaderStageKind_RayHitClosest; break;
    case DOT_FNV1A_5(RN_EXT_RMISS): match = string8_lit(RN_EXT_RMISS); shader_kind = RN_ShaderStageKind_RayHitMiss; break;
    case DOT_FNV1A_4(RN_EXT_RINT):  match = string8_lit(RN_EXT_RINT);  shader_kind = RN_ShaderStageKind_RayHitIntersection; break;
    case DOT_FNV1A_4(RN_EXT_CALL):  match = string8_lit(RN_EXT_CALL);  shader_kind = RN_ShaderStageKind_Callable; break;

    case DOT_FNV1A_4(RN_EXT_MESH):  match = string8_lit(RN_EXT_MESH);  shader_kind = RN_ShaderStageKind_Mesh; break;
    case DOT_FNV1A_4(RN_EXT_TASK):  match = string8_lit(RN_EXT_TASK);  shader_kind = RN_ShaderStageKind_Task; break;

    case DOT_FNV1A_4(RN_EXT_TESC):  match = string8_lit(RN_EXT_TESC);  shader_kind = RN_ShaderStageKind_TesselationControl; break;
    case DOT_FNV1A_4(RN_EXT_TESE):  match = string8_lit(RN_EXT_TESE);  shader_kind = RN_ShaderStageKind_TesselationEvaluation; break;
    case DOT_FNV1A_4(RN_EXT_GEOM):  match = string8_lit(RN_EXT_GEOM);  shader_kind = RN_ShaderStageKind_Geometry; break;
    }

    if(RN_ShaderStageKind_None == shader_kind){
        DOT_WARNING("Ambiguous file extension name '%S', returning RN_ShaderStageKind_Fragment! Try specifying RN_ShaderStageKind", ext);
        return(RN_ShaderStageKind_Fragment);
    }

    // (jd) Make sure we match the string properly
    if(string8_equal(ext, match)){
        return(shader_kind);
    }

    DOT_WARNING("Possible false negative hash collision for %S reconsider the approach", match);
    // (jd) If the collision is a false negative, we can still recover
    for (u32 i = 0; i < DOT_ARRAY_COUNT(rn_g_shader_stage_ext_map); i++) {
        if (string8_equal(ext, rn_g_shader_stage_ext_map[i].ext)) {
            shader_kind = rn_g_shader_stage_ext_map[i].kind;
            break;
        }
    }
    if(shader_kind == RN_ShaderStageKind_None){
        DOT_ERROR("Hash collision for %S, consider using another hash function", match);
    }
    return(shader_kind);
}

RN_ShaderStageKind
rn_shader_stage_kind_from_path(String8 path)
{
    String8 ext = string8_file_extension(path);
    RN_ShaderStageKind kind = rn_shader_stage_kind_from_ext(ext);
    return(kind);
}

internal String8
rn_slang_stage_from_shader_stage_kind(RN_ShaderStageKind kind)
{
    DOT_ALLOW_PARTIAL_SWITCH
    switch(kind){
    case RN_ShaderStageKind_Vertex:                return string8_lit("vertex");
    case RN_ShaderStageKind_Fragment:              return string8_lit("fragment");
    case RN_ShaderStageKind_Compute:               return string8_lit("compute");

    case RN_ShaderStageKind_RayGen:                return string8_lit("raygen");
    case RN_ShaderStageKind_RayHitAny:             return string8_lit("anyhit");
    case RN_ShaderStageKind_RayHitClosest:         return string8_lit("closesthit");
    case RN_ShaderStageKind_RayHitMiss:            return string8_lit("miss");
    case RN_ShaderStageKind_RayHitIntersection:    return string8_lit("intersection");
    case RN_ShaderStageKind_Callable:              return string8_lit("callable");

    case RN_ShaderStageKind_Mesh:                  return string8_lit("mesh");
    case RN_ShaderStageKind_Task:                  return string8_lit("amplification");

    case RN_ShaderStageKind_TesselationControl:    return string8_lit("hull");
    case RN_ShaderStageKind_TesselationEvaluation: return string8_lit("domain");
    case RN_ShaderStageKind_Geometry:              return string8_lit("geometry");
    default: return string8_lit("compute"); // fallback
    }
    DOT_RESTORE_PARTIAL_SWITCH
}
