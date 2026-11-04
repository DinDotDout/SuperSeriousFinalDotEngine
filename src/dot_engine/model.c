////////////////////////////////////////////////////////////////
//
// glTF

internal f32
dot_model_sampler_anisotropy(RB_SamplerDesc *desc)
{
    b32 trilinear = desc->min_filter == RB_SamplerFilterKind_Linear && desc->mipmap_filter == RB_SamplerMipmapFilterKind_Linear;
    return(trilinear ? rn_g_max_anisotropy : 1.0f);
}

internal RB_GpuAddress
dot_model_upload(RN_RenderCtx *renderer, void *data, i64 size)
{
    RB_GpuAddress addr = rn_data_alloc(renderer, size);
    rn_upload_memory(renderer, addr, data, size);
    return(addr);
}

internal DOT_Model
dot_model_from_gltf(RN_RenderCtx *renderer, const cgltf_data *gltf_data, String8 gltf_path)
{
    DOT_ASSERT(gltf_data->scene, "No default scene");
    DOT_ASSERT(gltf_data->textures_count <= U32_MAX);
    DOT_ASSERT(gltf_data->samplers_count <= U32_MAX);

    Scratch s = scratch_begin(0,0);

    DOT_Model model = {0};

    // Indexed by image (materials resolve textures through cgltf_image_index).
    model.texture_count = cast(u32)gltf_data->images_count;
    model.textures       = PUSH_ARRAY(renderer->permanent_arena, RB_Texture, model.texture_count);
    u32 *texture_slots   = PUSH_ARRAY(s.arena, u32, model.texture_count);

    // One submesh per primitive of the root scene's nodes (the loop below), not one per node.
    u64 primitive_count = 0;
    for(cgltf_size ri = 0; ri < gltf_data->scene->nodes_count; ++ri){
        cgltf_mesh *mesh = gltf_data->scene->nodes[ri]->mesh;
        primitive_count += mesh ? mesh->primitives_count : 0;
    }
    model.sub_mesh_draw = PUSH_ARRAY(renderer->permanent_arena, DOT_Submesh, primitive_count);

    String8 folder = string8_chop_last_slash(gltf_path);
    String8 model_name = string8_skip_last_slash(gltf_path);
    for(i64 c = 0; c < model_name.size; ++c){
        if(model_name.str[c] == '.'){ model_name.size = c; break; }
    }

    // (jd) NOTE: For now we are going to assume that all images are on a separate path
    for(u64 i = 0; i < gltf_data->images_count; ++i){
        cgltf_image *image = &gltf_data->images[i];
        String8 name = string8_format(s.arena, "%S:%llu", model_name, i);
        String8 full_path = string8_format(s.arena, "%S/%s", folder, image->uri);
        DOT_PRINT("name: %S, image uri: %S", name, full_path);

        DOT_PROFILE_BLOCK("Texture load"){
            RN_BindlessTexture loaded = rn_texture_load_from_path(renderer, name, full_path, 0);
            model.textures[i] = loaded.handle;
            texture_slots[i]  = loaded.bindless_slot;
        }
    }

    u32 *sampler_slots = PUSH_ARRAY(s.arena, u32, gltf_data->samplers_count);
    for(u64 i = 0; i < gltf_data->samplers_count; ++i){
        cgltf_sampler *sampler = &gltf_data->samplers[i];
        String8 name = sampler->name ? string8_from_cstring(sampler->name) : str_comp("Default");

        RB_SamplerDesc desc = {
            .debug_name = name,
            .mag_filter = sampler->mag_filter == cgltf_filter_type_linear
                ? RB_SamplerFilterKind_Linear
                : RB_SamplerFilterKind_Nearest,
        };

        switch(sampler->min_filter){
        case cgltf_filter_type_undefined:
        case cgltf_filter_type_nearest:
            desc.min_filter = RB_SamplerFilterKind_Nearest;
        break;
        case cgltf_filter_type_linear:
            desc.min_filter = RB_SamplerFilterKind_Linear;
        break;
        case cgltf_filter_type_linear_mipmap_nearest:
            desc.min_filter = RB_SamplerFilterKind_Linear;
            desc.mipmap_filter = RB_SamplerMipmapFilterKind_Nearest;
        break;
        case cgltf_filter_type_linear_mipmap_linear:
            desc.min_filter = RB_SamplerFilterKind_Linear;
            desc.mipmap_filter = RB_SamplerMipmapFilterKind_Linear;
        break;
        case cgltf_filter_type_nearest_mipmap_nearest:
            desc.min_filter = RB_SamplerFilterKind_Nearest;
            desc.mipmap_filter = RB_SamplerMipmapFilterKind_Nearest;
        break;
        case cgltf_filter_type_nearest_mipmap_linear:
            desc.min_filter = RB_SamplerFilterKind_Nearest;
            desc.mipmap_filter = RB_SamplerMipmapFilterKind_Linear;
        break;
        }

        switch(sampler->wrap_s){
        case cgltf_wrap_mode_clamp_to_edge  : desc.address_mode_u = RB_SamplerAddressModeKind_ClampToEdge; break;
        case cgltf_wrap_mode_mirrored_repeat: desc.address_mode_u = RB_SamplerAddressModeKind_MirroredRepeat; break;
        case cgltf_wrap_mode_repeat         : desc.address_mode_u = RB_SamplerAddressModeKind_Repeat; break;
        }
        switch(sampler->wrap_t){
        case cgltf_wrap_mode_clamp_to_edge  : desc.address_mode_v = RB_SamplerAddressModeKind_ClampToEdge; break;
        case cgltf_wrap_mode_mirrored_repeat: desc.address_mode_v = RB_SamplerAddressModeKind_MirroredRepeat; break;
        case cgltf_wrap_mode_repeat         : desc.address_mode_v = RB_SamplerAddressModeKind_Repeat; break;
        }

        desc.max_anisotropy = dot_model_sampler_anisotropy(&desc);
        sampler_slots[i] = rn_sampler_bindless_slot_create(renderer, &desc);
    }

    cgltf_scene *root_scene = gltf_data->scene;

    rn_upload_begin(renderer);

    DOT_PROFILE_BLOCK("Geometry and materials")
    for(cgltf_size ri = 0; ri < root_scene->nodes_count; ++ri){
        cgltf_node *gltf_node = root_scene->nodes[ri];
        if(gltf_node->mesh == NULL){
            DOT_TODO("Handle camera / light nodes");
            continue;
        }

        cgltf_mesh *gltf_mesh = gltf_node->mesh;

        f32x4x4 node_transform;
        cgltf_node_transform_world(gltf_node, cast(cgltf_float*)&node_transform);

        for(u32 primitive_index = 0; primitive_index < gltf_mesh->primitives_count; ++primitive_index){
            cgltf_primitive *primitive = &gltf_mesh->primitives[primitive_index];
            const cgltf_accessor *index_accessor     = primitive->indices;
            const cgltf_accessor *position_accessor  = cgltf_find_accessor(primitive, cgltf_attribute_type_position, 0);
            const cgltf_accessor *tangent_accessor   = cgltf_find_accessor(primitive, cgltf_attribute_type_tangent, 0);
            const cgltf_accessor *normal_accessor    = cgltf_find_accessor(primitive, cgltf_attribute_type_normal, 0);
            const cgltf_accessor *texcoord0_accessor = cgltf_find_accessor(primitive, cgltf_attribute_type_texcoord, 0);

            DOT_ASSERT(index_accessor, "No index data found");
            DOT_ASSERT(position_accessor, "No position data found");
            DOT_ASSERT(normal_accessor, "No normal data found - fallback generation isn't implemented");
            DOT_ASSERT(texcoord0_accessor, "No texcoord data found - fallback generation isn't implemented");

            DOT_Submesh *sub_mesh = &model.sub_mesh_draw[model.sub_mesh_draw_count];
            sub_mesh->transform = node_transform;

            DOT_ASSERT(index_accessor->count <= U32_MAX);
            u32 index_count = cast(u32)index_accessor->count;
            sub_mesh->index_count = index_count;

            u32 *indices_u32 = PUSH_ARRAY(s.arena, u32, index_count);
            cgltf_accessor_unpack_indices(index_accessor, indices_u32, sizeof(u32), index_count);
            sub_mesh->index_addr = dot_model_upload(renderer, indices_u32, index_count*sizeof(u32));

            u32 vertex_count = cast(u32)position_accessor->count;

            f32x3 *positions = PUSH_ARRAY(s.arena, f32x3, vertex_count);
            cgltf_accessor_unpack_floats(position_accessor, cast(f32 *)positions, vertex_count*3);
            sub_mesh->position_addr = dot_model_upload(renderer, positions, cast(i64)(vertex_count*sizeof(f32x3)));

            f32x3 *normals = PUSH_ARRAY(s.arena, f32x3, vertex_count);
            cgltf_accessor_unpack_floats(normal_accessor, cast(f32*)normals, vertex_count*3);
            sub_mesh->normal_addr = dot_model_upload(renderer, cast(f32 *)normals, cast(i64)(vertex_count*sizeof(f32x3)));

            f32x2 *texcoords = PUSH_ARRAY(s.arena, f32x2, vertex_count);
            cgltf_accessor_unpack_floats(texcoord0_accessor, cast(f32*)texcoords, vertex_count*2);
            sub_mesh->texcoord_addr = dot_model_upload(renderer, texcoords, cast(i64)(vertex_count*sizeof(f32x2)));

            if(tangent_accessor){
                f32x4 *tangents = PUSH_ARRAY(s.arena, f32x4, vertex_count);
                cgltf_accessor_unpack_floats(tangent_accessor, cast(f32*)tangents, vertex_count*4);
                sub_mesh->tangent_addr = dot_model_upload(renderer, tangents, cast(i64)(vertex_count*sizeof(f32x4)));
            }

            // Build the material
            DOT_MaterialGpu mat_gpu = {0};
            mat_gpu.base_color_factor = v4(1,1,1,1);
            mat_gpu.metallic_factor   = 1.0f;
            mat_gpu.roughness_factor  = 1.0f;
            mat_gpu.occlusion_factor  = 1.0f;

            cgltf_material *gltf_material = primitive->material;
            if(gltf_material && gltf_material->has_pbr_metallic_roughness){
                cgltf_pbr_metallic_roughness *pbr = &gltf_material->pbr_metallic_roughness;
                mat_gpu.base_color_factor = v4(pbr->base_color_factor[0], pbr->base_color_factor[1],
                    pbr->base_color_factor[2], pbr->base_color_factor[3]);
                mat_gpu.metallic_factor  = pbr->metallic_factor;
                mat_gpu.roughness_factor = pbr->roughness_factor;

                if(pbr->base_color_texture.texture && pbr->base_color_texture.texture->image){
                    u32 tex_i = cast(u32)cgltf_image_index(gltf_data, pbr->base_color_texture.texture->image);
                    mat_gpu.tex_slots[DOT_PBRTextureKind_Diffuse] = texture_slots[tex_i];
                    if(pbr->base_color_texture.texture->sampler){
                        u32 samp_i = cast(u32)cgltf_sampler_index(gltf_data, pbr->base_color_texture.texture->sampler);
                        mat_gpu.sampler_slots[DOT_PBRTextureKind_Diffuse] = sampler_slots[samp_i];
                    }
                    mat_gpu.flags |= DOT_MaterialFeatureBit_ColorTexture;
                }
            }
            if(gltf_material){
                cgltf_texture_view *emissive_view = &gltf_material->emissive_texture;
                mat_gpu.emissive_factor = v3(gltf_material->emissive_factor[0], gltf_material->emissive_factor[1], gltf_material->emissive_factor[2]);
                if(emissive_view->texture && emissive_view->texture->image){
                    u32 tex_i = cast(u32)cgltf_image_index(gltf_data, emissive_view->texture->image);
                    mat_gpu.tex_slots[DOT_PBRTextureKind_Emissive] = texture_slots[tex_i];
                    if(emissive_view->texture->sampler){
                        u32 samp_i = cast(u32)cgltf_sampler_index(gltf_data, emissive_view->texture->sampler);
                        mat_gpu.sampler_slots[DOT_PBRTextureKind_Emissive] = sampler_slots[samp_i];
                    }
                    mat_gpu.flags |= DOT_MaterialFeatureBit_EmissiveTexture;
                }

                cgltf_texture_view *occlusion_view = &gltf_material->occlusion_texture;
                if(occlusion_view->texture && occlusion_view->texture->image){
                    u32 tex_i = cast(u32)cgltf_image_index(gltf_data, occlusion_view->texture->image);
                    mat_gpu.tex_slots[DOT_PBRTextureKind_Occlusion] = texture_slots[tex_i];
                    if(occlusion_view->texture->sampler){
                        u32 samp_i = cast(u32)cgltf_sampler_index(gltf_data, occlusion_view->texture->sampler);
                        mat_gpu.sampler_slots[DOT_PBRTextureKind_Occlusion] = sampler_slots[samp_i];
                    }
                    mat_gpu.occlusion_factor = occlusion_view->scale;
                    mat_gpu.flags |= DOT_MaterialFeatureBit_OcclusionTexture;
                }

                cgltf_texture_view *normal_view = &gltf_material->normal_texture;
                if(normal_view->texture && normal_view->texture->image){
                    u32 tex_i = cast(u32)cgltf_image_index(gltf_data, normal_view->texture->image);
                    mat_gpu.tex_slots[DOT_PBRTextureKind_Normal] = texture_slots[tex_i];
                    if(normal_view->texture->sampler){
                        u32 samp_i = cast(u32)cgltf_sampler_index(gltf_data, normal_view->texture->sampler);
                        mat_gpu.sampler_slots[DOT_PBRTextureKind_Normal] = sampler_slots[samp_i];
                    }
                    mat_gpu.flags |= DOT_MaterialFeatureBit_NormalTexture;
                }
            }

            sub_mesh->material_addr = dot_model_upload(renderer, &mat_gpu, sizeof(mat_gpu));
            model.sub_mesh_draw_count += 1;
        }
    }

    DOT_PROFILE_BLOCK("Geometry upload submit"){ rn_upload_end(renderer); }

    scratch_end(s);
    return(model);
}

internal cgltf_data *
dot_gltf_load_from_path(Arena *arena, String8 path)
{
    String8 buff = platform_read_entire_file(arena, path);

    cgltf_options options = {
        .memory = {
            .alloc_func = dot_cgltf_alloc,
            .free_func  = dot_cgltf_free,
            .user_data  = arena,
        }
    };

    cgltf_data *gltf_data = NULL;
    cgltf_result result = cgltf_result_success;
    DOT_PROFILE_BLOCK("glTF parse"){ result = cgltf_parse(&options, buff.str, cast(u64)buff.size, &gltf_data); }
    b32 fail_parse = false;
    if(result != cgltf_result_success){
        fail_parse = true;
        DOT_WARNING("cgltf_parse failed: %d", result);
    }
    b32 load_buffers_failed = false;
    if(!fail_parse){
        DOT_PROFILE_BLOCK("glTF load buffers"){ result = cgltf_load_buffers(&options, gltf_data, path.cstr); }
        load_buffers_failed = result != cgltf_result_success;
    }
    if(load_buffers_failed){
        DOT_WARNING("cgltf_load_buffers failed: %d", result);
        gltf_data = NULL;
    }
	return(gltf_data);
}

internal DOT_Model
dot_model_load_from_path(RN_RenderCtx *renderer, String8 path)
{
    Scratch s = scratch_begin(0,0);
    cgltf_data *gltf = dot_gltf_load_from_path(s.arena, path);
    DOT_Model model = {0};
    if(gltf){
        model = dot_model_from_gltf(renderer, gltf, path);
        // cgltf_free(gltf);
    }
    scratch_end(s);
	return(model);
}

internal void
dot_model_destroy(RN_RenderCtx *renderer, DOT_Model *model)
{
    for EACH_INDEX(i, model->texture_count){
        rn_texture_destroy(renderer, model->textures[i]);
    }
}

////////////////////////////////////////////////////////////////
//
// Bake (glTF + images -> .dotm)

typedef struct DOT_ModelBakeWriter
{
    PlatformFile file;
    u64          cursor;
    b32          ok;
    Arena       *blocks_arena;
    HashMap(DOT_Hash128, u64) blocks;
    u64          deduped_bytes;
}DOT_ModelBakeWriter;

typedef struct DOT_ModelBakeGpu
{
    b32                  ready;
    RB_Pipeline          bc7_try_mode_456;
    RB_Pipeline          bc7_try_mode_137;
    RB_Pipeline          bc7_try_mode_02;
    RB_Pipeline          bc7_encode_block;
    RB_Pipeline          bc5_encode;
    RB_Pipeline          mip_downsample;
    RB_CommandPool       pool;
    RB_TimelineSemaphore timeline;
    u64                  timeline_value;
    RB_CpuGpuArena       input_heap;
    RB_GpuArena          mip_heap;
    RB_GpuArena          candidate_heap;
    RB_CpuGpuArena       readback_heap;
}DOT_ModelBakeGpu;

typedef struct DOT_ModelBakeBlocks
{
    u32           mip_width[DOT_MODEL_FILE_MAX_MIPS];
    u32           mip_height[DOT_MODEL_FILE_MAX_MIPS];
    u32           mip_blocks_x[DOT_MODEL_FILE_MAX_MIPS];
    u32           mip_blocks[DOT_MODEL_FILE_MAX_MIPS];
    u64           mip_first_block[DOT_MODEL_FILE_MAX_MIPS];
    RB_GpuAddress mip_pixels[DOT_MODEL_FILE_MAX_MIPS];
    u64           total_blocks;
    u64           total_batches;
    i64           mip0_size;
    i64           mip_chain_size;
    RB_CpuGpuSpan readback;
}DOT_ModelBakeBlocks;

typedef u32 DOT_ModelBakeTextureUse;
enum
{
    DOT_ModelBakeTextureUse_Color  = 1 << 0,
    DOT_ModelBakeTextureUse_Normal = 1 << 1,
    DOT_ModelBakeTextureUse_Data   = 1 << 2,
};

internal DOT_Hash128
dot_hash128_from_data(void const *key, u64 key_size, void const *data, u64 size)
{
    XXH3_state_t state;
    XXH3_128bits_reset(&state);
    XXH3_128bits_update(&state, key, key_size);
    XXH3_128bits_update(&state, data, size);
    XXH128_hash_t hash = XXH3_128bits_digest(&state);
    return((DOT_Hash128){ .lo = hash.low64, .hi = hash.high64 });
}

internal String8
dot_model_baked_path(Arena *arena, String8 source_path)
{
    String8 name = string8_skip_last_slash(source_path);
    for(i64 c = 0; c < name.size; ++c){
        if(name.str[c] == '.'){ name.size = c; break; }
    }
    u64 path_hash = hash_fnv1a_u64(source_path.str, cast(u64)source_path.size);
    return(string8_format(arena, "%s%S-%016" PRIx64 ".dotm", DOT_MODEL_CACHE_PATH, name, path_hash));
}

internal u64
dot_model_bake_place(u64 *cursor, u64 size, u64 align)
{
    u64 offset = ALIGN_POW2(*cursor, align);
    *cursor = offset + size;
    return(offset);
}

internal u64
dot_model_bake_write_block(DOT_ModelBakeWriter *writer, DOT_Hash128 hash, void const *data, u64 size)
{
    u64 *written = hm_get(&writer->blocks, hash);
    if(written){
        writer->deduped_bytes += size;
        return(*written);
    }
    u64 offset = dot_model_bake_place(&writer->cursor, size, DOT_MODEL_FILE_ALIGN);
    writer->ok = writer->ok && platform_file_write_at(&writer->file, cast(i64)offset, data, cast(i64)size);
    *hm_get_or_create(writer->blocks_arena, &writer->blocks, hash) = offset;
    return(offset);
}

internal b32
dot_model_bake_mesh(Arena *arena, DOT_ModelBakeWriter *writer, cgltf_primitive *primitive, DOT_ModelFileMesh *out)
{
    const cgltf_accessor *index_accessor    = primitive->indices;
    const cgltf_accessor *position_accessor = cgltf_find_accessor(primitive, cgltf_attribute_type_position, 0);
    const cgltf_accessor *normal_accessor   = cgltf_find_accessor(primitive, cgltf_attribute_type_normal, 0);
    const cgltf_accessor *texcoord_accessor = cgltf_find_accessor(primitive, cgltf_attribute_type_texcoord, 0);
    const cgltf_accessor *tangent_accessor  = cgltf_find_accessor(primitive, cgltf_attribute_type_tangent, 0);
    if(!index_accessor || !position_accessor || !normal_accessor || !texcoord_accessor){
        DOT_WARNING("Skipping primitive without indices, positions, normals or texcoords");
        return(false);
    }
    DOT_ASSERT(index_accessor->count <= U32_MAX && position_accessor->count <= U32_MAX);

    TempArena temp = temp_arena_start(arena);
    out->index_count  = cast(u32)index_accessor->count;
    out->vertex_count = cast(u32)position_accessor->count;
    out->has_tangents = tangent_accessor != NULL;

    u64 cursor = 0;
    out->index_offset    = dot_model_bake_place(&cursor, out->index_count  * sizeof(u32),   DOT_MODEL_FILE_ALIGN);
    out->position_offset = dot_model_bake_place(&cursor, out->vertex_count * sizeof(f32x3), DOT_MODEL_FILE_ALIGN);
    out->normal_offset   = dot_model_bake_place(&cursor, out->vertex_count * sizeof(f32x3), DOT_MODEL_FILE_ALIGN);
    out->texcoord_offset = dot_model_bake_place(&cursor, out->vertex_count * sizeof(f32x2), DOT_MODEL_FILE_ALIGN);
    if(out->has_tangents){
        out->tangent_offset = dot_model_bake_place(&cursor, out->vertex_count * sizeof(f32x4), DOT_MODEL_FILE_ALIGN);
    }
    out->size = cursor;

    u8 *block = PUSH_ARRAY(temp.arena, u8, out->size);
    cgltf_accessor_unpack_indices(index_accessor, block + out->index_offset, sizeof(u32), out->index_count);
    cgltf_accessor_unpack_floats(position_accessor, cast(f32*)(block + out->position_offset), out->vertex_count * 3);
    cgltf_accessor_unpack_floats(normal_accessor,   cast(f32*)(block + out->normal_offset),   out->vertex_count * 3);
    cgltf_accessor_unpack_floats(texcoord_accessor, cast(f32*)(block + out->texcoord_offset), out->vertex_count * 2);
    if(out->has_tangents){
        cgltf_accessor_unpack_floats(tangent_accessor, cast(f32*)(block + out->tangent_offset), out->vertex_count * 4);
    }

    DOT_ModelFileMeshKey key = { .kind = 1, .index_count = out->index_count, .vertex_count = out->vertex_count, .has_tangents = out->has_tangents };
    out->hash   = dot_hash128_from_data(&key, sizeof(key), block, out->size);
    out->offset = dot_model_bake_write_block(writer, out->hash, block, out->size);
    temp_arena_restore(temp);
    return(true);
}

DOT_SETTING(b32,"Bake", dot_g_bake_bc7_three_subsets, false);
DOT_SETTING(b32,"Bake", dot_g_bake_report_bc5_quality, false);

#define DOT_BC_BLOCK_DIM                4
#define DOT_BC_BLOCK_PIXELS             (DOT_BC_BLOCK_DIM * DOT_BC_BLOCK_DIM)
#define DOT_BC4_BLOCK_BYTES             8
#define DOT_BC4_ENDPOINT_BYTES          2
#define DOT_BC4_INDEX_BYTES             (DOT_BC4_BLOCK_BYTES - DOT_BC4_ENDPOINT_BYTES)
#define DOT_BC4_INDEX_BITS              3
#define DOT_BC4_INDEX_MASK              ((1u << DOT_BC4_INDEX_BITS) - 1)
#define DOT_BC4_PALETTE_SIZE            8
#define DOT_BC5_CHANNELS                2
#define DOT_RGBA8_CHANNELS              4

#define DOT_MODEL_BAKE_BC_BLOCK_SIZE    16
#define DOT_MODEL_BAKE_BC7_BATCH_BLOCKS 65532
#define DOT_MODEL_BAKE_BC7_GROUP_BLOCKS 4
#define DOT_MODEL_BAKE_BC5_GROUP_BLOCKS 64
#define DOT_MODEL_BAKE_BC5_BATCH_BLOCKS (65535 * DOT_MODEL_BAKE_BC5_GROUP_BLOCKS)

internal b32
dot_model_bake_gpu_init(RN_RenderCtx *renderer, DOT_ModelBakeGpu *gpu)
{
    String8 bc7_path = str_comp(RN_SHADER_PATH "bc7_encode.comp");
    gpu->bc7_try_mode_456 = rn_compute_pipeline_create(renderer, bc7_path, str_comp("TryMode456CS"));
    gpu->bc7_try_mode_137 = rn_compute_pipeline_create(renderer, bc7_path, str_comp("TryMode137CS"));
    gpu->bc7_try_mode_02  = rn_compute_pipeline_create(renderer, bc7_path, str_comp("TryMode02CS"));
    gpu->bc7_encode_block = rn_compute_pipeline_create(renderer, bc7_path, str_comp("EncodeBlockCS"));
    gpu->bc5_encode       = rn_compute_pipeline_create(renderer, str_comp(RN_SHADER_PATH "bc5_encode.comp"), (String8){0});
    gpu->mip_downsample   = rn_compute_pipeline_create(renderer, str_comp(RN_SHADER_PATH "mip_downsample.comp"), (String8){0});
    gpu->pool             = rb_command_pool_create();
    gpu->timeline         = rb_timeline_create(0);
    gpu->ready = RB_HANDLE_VALID(gpu->bc7_try_mode_456) && RB_HANDLE_VALID(gpu->bc7_try_mode_137)
              && RB_HANDLE_VALID(gpu->bc7_try_mode_02)  && RB_HANDLE_VALID(gpu->bc7_encode_block)
              && RB_HANDLE_VALID(gpu->bc5_encode)       && RB_HANDLE_VALID(gpu->mip_downsample);
    if(!gpu->ready){
        DOT_WARNING("Could not create the BC encode pipelines, baking textures uncompressed");
    }
    return(gpu->ready);
}

internal void
dot_model_bake_gpu_destroy(RN_RenderCtx *renderer, DOT_ModelBakeGpu *gpu)
{
    RB_Pipeline pipelines[] = { gpu->bc7_try_mode_456, gpu->bc7_try_mode_137, gpu->bc7_try_mode_02, gpu->bc7_encode_block, gpu->bc5_encode, gpu->mip_downsample };
    for EACH_INDEX(i, DOT_ARRAY_COUNT(pipelines)){
        if(RB_HANDLE_VALID(pipelines[i])){
            rn_pipeline_destroy(renderer, pipelines[i]);
        }
    }
    rb_command_pool_destroy(gpu->pool);
    rb_timeline_destroy(gpu->timeline);
    if(gpu->input_heap.size){
        rb_cpu_gpu_arena_destroy(&gpu->input_heap);
    }
    if(gpu->mip_heap.size){
        rb_gpu_arena_destroy(&gpu->mip_heap);
    }
    if(gpu->candidate_heap.size){
        rb_gpu_arena_destroy(&gpu->candidate_heap);
    }
    if(gpu->readback_heap.size){
        rb_cpu_gpu_arena_destroy(&gpu->readback_heap);
    }
}

internal void
dot_model_bake_gpu_reserve(DOT_ModelBakeGpu *gpu, i64 input_size, i64 mip_size, i64 candidate_size, i64 readback_size)
{
    if(gpu->input_heap.size < input_size){
        if(gpu->input_heap.size){
            rb_cpu_gpu_arena_destroy(&gpu->input_heap);
        }
        gpu->input_heap = rb_cpu_gpu_arena_create(RB_MemoryKind_CpuToGpu, input_size);
    }
    if(mip_size && gpu->mip_heap.size < mip_size){
        if(gpu->mip_heap.size){
            rb_gpu_arena_destroy(&gpu->mip_heap);
        }
        gpu->mip_heap = rb_gpu_arena_create(mip_size);
    }
    if(candidate_size && gpu->candidate_heap.size < candidate_size){
        if(gpu->candidate_heap.size){
            rb_gpu_arena_destroy(&gpu->candidate_heap);
        }
        gpu->candidate_heap = rb_gpu_arena_create(candidate_size);
    }
    if(gpu->readback_heap.size < readback_size){
        if(gpu->readback_heap.size){
            rb_cpu_gpu_arena_destroy(&gpu->readback_heap);
        }
        gpu->readback_heap = rb_cpu_gpu_arena_create(RB_MemoryKind_Readback, readback_size);
    }
    rb_cpu_gpu_arena_reset(&gpu->input_heap);
    gpu->mip_heap.used       = 0;
    gpu->candidate_heap.used = 0;
    rb_cpu_gpu_arena_reset(&gpu->readback_heap);
}

internal void
dot_model_bake_blocks_layout(DOT_ModelBakeBlocks *blocks, int width, int height, u8 mip_count, u32 batch_blocks, DOT_ModelFileTexture *out)
{
    out->width     = cast(u32)width;
    out->height    = cast(u32)height;
    out->mip_count = mip_count;
    u64 cursor = 0;
    for(u32 mip = 0; mip < mip_count; ++mip){
        blocks->mip_width[mip]       = cast(u32)DOT_MAX(width  >> mip, 1);
        blocks->mip_height[mip]      = cast(u32)DOT_MAX(height >> mip, 1);
        blocks->mip_blocks_x[mip]    = (blocks->mip_width[mip] + 3) / 4;
        blocks->mip_blocks[mip]      = blocks->mip_blocks_x[mip] * ((blocks->mip_height[mip] + 3) / 4);
        blocks->mip_first_block[mip] = blocks->total_blocks;
        blocks->total_blocks  += blocks->mip_blocks[mip];
        blocks->total_batches += (blocks->mip_blocks[mip] + batch_blocks - 1) / batch_blocks;
        i64 level_size = ALIGN_POW2(cast(i64)blocks->mip_width[mip] * blocks->mip_height[mip] * DOT_RGBA8_CHANNELS, cast(i64)RB_GPU_ARENA_ALIGN);
        if(mip == 0){
            blocks->mip0_size = level_size;
        }else{
            blocks->mip_chain_size += level_size;
        }
        out->mip_sizes[mip]   = cast(u64)blocks->mip_blocks[mip] * DOT_MODEL_BAKE_BC_BLOCK_SIZE;
        out->mip_offsets[mip] = dot_model_bake_place(&cursor, out->mip_sizes[mip], DOT_MODEL_FILE_ALIGN);
    }
    out->size = cursor;
}

internal void
dot_model_bake_blocks_upload(DOT_ModelBakeGpu *gpu, DOT_ModelBakeBlocks *blocks, u8 *rgba, u8 mip_count, i64 roots_size, i64 candidate_size, u64 readback_size)
{
    i64 downsample_roots_size = cast(i64)mip_count * ALIGN_POW2(cast(i64)sizeof(RN_MipDownsampleRoot), cast(i64)RB_GPU_ARENA_ALIGN);
    dot_model_bake_gpu_reserve(gpu, blocks->mip0_size + roots_size + downsample_roots_size, blocks->mip_chain_size, candidate_size, cast(i64)readback_size);
    blocks->readback = rb_cpu_gpu_arena_reserve_atomic(&gpu->readback_heap, cast(i64)readback_size);
    MEMORY_ZERO(blocks->readback.cpu, readback_size);

    i64 mip0_size = cast(i64)blocks->mip_width[0] * blocks->mip_height[0] * DOT_RGBA8_CHANNELS;
    RB_CpuGpuSpan mip0 = rb_cpu_gpu_arena_reserve_atomic(&gpu->input_heap, mip0_size);
    MEMORY_COPY(mip0.cpu, rgba, cast(u64)mip0_size);
    blocks->mip_pixels[0] = mip0.gpu.gpu;
    for(u32 mip = 1; mip < mip_count; ++mip){
        i64 level_size = cast(i64)blocks->mip_width[mip] * blocks->mip_height[mip] * DOT_RGBA8_CHANNELS;
        blocks->mip_pixels[mip] = rb_gpu_arena_alloc_atomic(&gpu->mip_heap, level_size).gpu;
    }
}

internal void
dot_model_bake_blocks_downsample(DOT_ModelBakeGpu *gpu, RB_CommandBuffer cmd, DOT_ModelBakeBlocks *blocks, b32 srgb, u8 mip_count)
{
    if(mip_count <= 1){
        return;
    }
    rb_bind_pso(cmd, gpu->mip_downsample);
    for(u32 mip = 1; mip < mip_count; ++mip){
        RN_MipDownsampleRoot root = {0};
        root.src        = blocks->mip_pixels[mip - 1];
        root.dst        = blocks->mip_pixels[mip];
        root.src_width  = blocks->mip_width[mip - 1];
        root.src_height = blocks->mip_height[mip - 1];
        root.dst_width  = blocks->mip_width[mip];
        root.dst_height = blocks->mip_height[mip];
        root.srgb       = srgb;
        RB_GpuRange root_range = rb_cpu_gpu_arena_push_atomic(&gpu->input_heap, root);
        u32 groups_x = (root.dst_width  + RN_MIP_DOWNSAMPLE_GROUP_DIM - 1) / RN_MIP_DOWNSAMPLE_GROUP_DIM;
        u32 groups_y = (root.dst_height + RN_MIP_DOWNSAMPLE_GROUP_DIM - 1) / RN_MIP_DOWNSAMPLE_GROUP_DIM;
        rb_dispatch(cmd, root_range.gpu, groups_x, groups_y, 1);
        rb_barrier(cmd, RB_StageBit_Compute, RB_AccessBit_ShaderWrite, RB_StageBit_Compute, RB_AccessBit_ShaderRead);
    }
}

internal void
dot_model_bake_gpu_submit_and_wait(DOT_ModelBakeGpu *gpu, RB_CommandBuffer cmd)
{
    gpu->timeline_value += 1;
    RB_TimelinePoint completion = { gpu->timeline, gpu->timeline_value };
    rb_submit(&(RB_SubmitDesc){ .command_buffers = &cmd, .command_buffer_count = 1, .completion = completion });
    rb_timeline_wait(completion);
}

typedef struct DOT_ModelBakeBC7Pass
{
    RB_Pipeline pipeline;
    u32         mode_id;
    u32         blocks_per_group;
}DOT_ModelBakeBC7Pass;

internal u8 *
dot_model_bake_bc7(DOT_ModelBakeGpu *gpu, u8 *rgba, int width, int height, b32 srgb, u8 mip_count, DOT_ModelFileTexture *out)
{
    out->format_kind = srgb ? RB_FormatKind_Bc7_Srgb : RB_FormatKind_Bc7_Unorm;

    DOT_ModelBakeBC7Pass passes[7];
    u32 pass_count = 0;
    passes[pass_count++] = (DOT_ModelBakeBC7Pass){ gpu->bc7_try_mode_456, 0, DOT_MODEL_BAKE_BC7_GROUP_BLOCKS };
    passes[pass_count++] = (DOT_ModelBakeBC7Pass){ gpu->bc7_try_mode_137, 1, 1 };
    passes[pass_count++] = (DOT_ModelBakeBC7Pass){ gpu->bc7_try_mode_137, 3, 1 };
    passes[pass_count++] = (DOT_ModelBakeBC7Pass){ gpu->bc7_try_mode_137, 7, 1 };
    if(dot_g_bake_bc7_three_subsets){
        passes[pass_count++] = (DOT_ModelBakeBC7Pass){ gpu->bc7_try_mode_02, 0, 1 };
        passes[pass_count++] = (DOT_ModelBakeBC7Pass){ gpu->bc7_try_mode_02, 2, 1 };
    }
    passes[pass_count++] = (DOT_ModelBakeBC7Pass){ gpu->bc7_encode_block, 0, DOT_MODEL_BAKE_BC7_GROUP_BLOCKS };

    DOT_ModelBakeBlocks blocks = {0};
    dot_model_bake_blocks_layout(&blocks, width, height, mip_count, DOT_MODEL_BAKE_BC7_BATCH_BLOCKS, out);
    i64 roots_size     = cast(i64)(blocks.total_batches * pass_count) * ALIGN_POW2(cast(i64)sizeof(RN_BC7EncodeRoot), cast(i64)RB_GPU_ARENA_ALIGN);
    i64 candidate_size = cast(i64)(blocks.total_blocks + DOT_MODEL_BAKE_BC7_GROUP_BLOCKS) * DOT_MODEL_BAKE_BC_BLOCK_SIZE;
    dot_model_bake_blocks_upload(gpu, &blocks, rgba, mip_count, roots_size, candidate_size, out->size);
    RB_GpuAddress candidates = rb_gpu_arena_alloc_atomic(&gpu->candidate_heap, candidate_size).gpu;

    rb_command_pool_reset(gpu->pool);
    RB_CommandBuffer cmd = rb_commands_begin(gpu->pool);
    dot_model_bake_blocks_downsample(gpu, cmd, &blocks, srgb, mip_count);
    for(u32 p = 0; p < pass_count; ++p){
        DOT_ModelBakeBC7Pass pass = passes[p];
        b32 last = p == pass_count - 1;
        rb_bind_pso(cmd, pass.pipeline);
        for(u32 mip = 0; mip < mip_count; ++mip){
            RB_GpuAddress mip_candidates = candidates + blocks.mip_first_block[mip] * DOT_MODEL_BAKE_BC_BLOCK_SIZE;
            for(u32 start = 0; start < blocks.mip_blocks[mip]; start += DOT_MODEL_BAKE_BC7_BATCH_BLOCKS){
                u32 batch_blocks = DOT_MIN(blocks.mip_blocks[mip] - start, DOT_MODEL_BAKE_BC7_BATCH_BLOCKS);
                RN_BC7EncodeRoot root = {0};
                root.pixels           = blocks.mip_pixels[mip];
                root.in_buffer        = mip_candidates;
                root.out_buffer       = last ? blocks.readback.gpu.gpu + out->mip_offsets[mip] : mip_candidates;
                root.tex_width        = blocks.mip_width[mip];
                root.tex_height       = blocks.mip_height[mip];
                root.num_block_x      = blocks.mip_blocks_x[mip];
                root.mode_id          = pass.mode_id;
                root.start_block_id   = start;
                root.num_total_blocks = blocks.mip_blocks[mip];
                root.alpha_weight     = 1.0f;
                RB_GpuRange root_range = rb_cpu_gpu_arena_push_atomic(&gpu->input_heap, root);
                rb_dispatch(cmd, root_range.gpu, (batch_blocks + pass.blocks_per_group - 1) / pass.blocks_per_group, 1, 1);
            }
        }
        if(last){
            rb_barrier(cmd, RB_StageBit_Compute, RB_AccessBit_ShaderWrite, RB_StageBit_Host, RB_AccessBit_HostRead);
        }else{
            rb_barrier(cmd, RB_StageBit_Compute, RB_AccessBit_ShaderWrite, RB_StageBit_Compute, RB_AccessBit_ShaderRead | RB_AccessBit_ShaderWrite);
        }
    }
    dot_model_bake_gpu_submit_and_wait(gpu, cmd);
    return(cast(u8*)blocks.readback.cpu);
}

internal u8 *
dot_model_bake_bc5(DOT_ModelBakeGpu *gpu, u8 *rgba, int width, int height, u8 mip_count, DOT_ModelFileTexture *out)
{
    out->format_kind = RB_FormatKind_Bc5_Rg;

    DOT_ModelBakeBlocks blocks = {0};
    dot_model_bake_blocks_layout(&blocks, width, height, mip_count, DOT_MODEL_BAKE_BC5_BATCH_BLOCKS, out);
    i64 roots_size = cast(i64)blocks.total_batches * ALIGN_POW2(cast(i64)sizeof(RN_BC5EncodeRoot), cast(i64)RB_GPU_ARENA_ALIGN);
    dot_model_bake_blocks_upload(gpu, &blocks, rgba, mip_count, roots_size, 0, out->size);

    rb_command_pool_reset(gpu->pool);
    RB_CommandBuffer cmd = rb_commands_begin(gpu->pool);
    dot_model_bake_blocks_downsample(gpu, cmd, &blocks, false, mip_count);
    rb_bind_pso(cmd, gpu->bc5_encode);
    for(u32 mip = 0; mip < mip_count; ++mip){
        for(u32 start = 0; start < blocks.mip_blocks[mip]; start += DOT_MODEL_BAKE_BC5_BATCH_BLOCKS){
            u32 batch_blocks = DOT_MIN(blocks.mip_blocks[mip] - start, DOT_MODEL_BAKE_BC5_BATCH_BLOCKS);
            RN_BC5EncodeRoot root = {0};
            root.pixels           = blocks.mip_pixels[mip];
            root.out_buffer       = blocks.readback.gpu.gpu + out->mip_offsets[mip];
            root.tex_width        = blocks.mip_width[mip];
            root.tex_height       = blocks.mip_height[mip];
            root.num_block_x      = blocks.mip_blocks_x[mip];
            root.start_block_id   = start;
            root.num_total_blocks = blocks.mip_blocks[mip];
            RB_GpuRange root_range = rb_cpu_gpu_arena_push_atomic(&gpu->input_heap, root);
            rb_dispatch(cmd, root_range.gpu, (batch_blocks + DOT_MODEL_BAKE_BC5_GROUP_BLOCKS - 1) / DOT_MODEL_BAKE_BC5_GROUP_BLOCKS, 1, 1);
        }
    }
    rb_barrier(cmd, RB_StageBit_Compute, RB_AccessBit_ShaderWrite, RB_StageBit_Host, RB_AccessBit_HostRead);
    dot_model_bake_gpu_submit_and_wait(gpu, cmd);
    return(cast(u8*)blocks.readback.cpu);
}

internal void
dot_model_bake_bc4_palette(u8 const *bc4_block, u32 palette[DOT_BC4_PALETTE_SIZE])
{
    u32 endpoint0 = bc4_block[0];
    u32 endpoint1 = bc4_block[1];
    palette[0] = endpoint0;
    palette[1] = endpoint1;
    if(endpoint0 > endpoint1){
        for(u32 i = 2; i < 8; ++i){
            palette[i] = ((8 - i) * endpoint0 + (i - 1) * endpoint1 + 3) / 7;
        }
    }else{
        for(u32 i = 2; i < 6; ++i){
            palette[i] = ((6 - i) * endpoint0 + (i - 1) * endpoint1 + 2) / 5;
        }
        palette[6] = 0;
        palette[7] = 255;
    }
}

internal void
dot_model_bake_bc4_decode_block(u8 const *bc4_block, u8 pixels[DOT_BC_BLOCK_PIXELS])
{
    u32 palette[DOT_BC4_PALETTE_SIZE];
    dot_model_bake_bc4_palette(bc4_block, palette);
    u64 indices = 0;
    for(u32 i = 0; i < DOT_BC4_INDEX_BYTES; ++i){
        indices |= cast(u64)bc4_block[DOT_BC4_ENDPOINT_BYTES + i] << (8 * i);
    }
    for(u32 pixel = 0; pixel < DOT_BC_BLOCK_PIXELS; ++pixel){
        u32 index = cast(u32)(indices >> (DOT_BC4_INDEX_BITS * pixel)) & DOT_BC4_INDEX_MASK;
        pixels[pixel] = cast(u8)palette[index];
    }
}

internal f64
dot_model_bake_bc5_psnr(u8 const *bc5_blocks, u8 const *rgba, u32 width, u32 height)
{
    u32 blocks_x = (width  + DOT_BC_BLOCK_DIM - 1) / DOT_BC_BLOCK_DIM;
    u32 blocks_y = (height + DOT_BC_BLOCK_DIM - 1) / DOT_BC_BLOCK_DIM;
    f64 squared_error_sum = 0.0;
    u64 sample_count      = 0;
    for(u32 block_y = 0; block_y < blocks_y; ++block_y){
        for(u32 block_x = 0; block_x < blocks_x; ++block_x){
            u8 const *bc5_block = bc5_blocks + (cast(u64)block_y * blocks_x + block_x) * DOT_MODEL_BAKE_BC_BLOCK_SIZE;
            for(u32 channel = 0; channel < DOT_BC5_CHANNELS; ++channel){
                u8 decoded[DOT_BC_BLOCK_PIXELS];
                dot_model_bake_bc4_decode_block(bc5_block + channel * DOT_BC4_BLOCK_BYTES, decoded);
                for(u32 pixel = 0; pixel < DOT_BC_BLOCK_PIXELS; ++pixel){
                    u32 x = DOT_MIN(block_x * DOT_BC_BLOCK_DIM + pixel % DOT_BC_BLOCK_DIM, width  - 1);
                    u32 y = DOT_MIN(block_y * DOT_BC_BLOCK_DIM + pixel / DOT_BC_BLOCK_DIM, height - 1);
                    i32 source = rgba[(cast(u64)y * width + x) * DOT_RGBA8_CHANNELS + channel];
                    i32 diff   = cast(i32)decoded[pixel] - source;
                    squared_error_sum += cast(f64)(diff * diff);
                    sample_count      += 1;
                }
            }
        }
    }
    f64 mean_squared_error = squared_error_sum / cast(f64)sample_count;
    return(mean_squared_error > 0.0 ? 10.0 * log10(255.0 * 255.0 / mean_squared_error) : INFINITY);
}

internal b32
dot_model_bake_texture(Arena *arena, DOT_ModelBakeWriter *writer, DOT_ModelBakeGpu *gpu, String8 path, DOT_ModelBakeTextureUse use, DOT_ModelFileTexture *out)
{
    b32 srgb = DOT_BITS_ANY(use, DOT_ModelBakeTextureUse_Color);
    TempArena temp = temp_arena_start(arena);
    String8 file_data = platform_read_entire_file(temp.arena, path);
    if(!file_data.size){
        temp_arena_restore(temp);
        return(false);
    }

    stbi_allocator alloc = {.alloc = rn_stbi_alloc, .realloc = rn_stbi_realloc, .user = temp.arena};
    stbi_allocator_set(&alloc);
    b32 is_hdr    = cast(b32)stbi_is_hdr_from_memory(file_data.str, cast(int)file_data.size);
    b32 is_16_bit = is_hdr || stbi_is_16_bit_from_memory(file_data.str, cast(int)file_data.size);
    int width  = 0;
    int height = 0;
    int comp   = 0;
    stbi_info_from_memory(file_data.str, cast(int)file_data.size, &width, &height, &comp);
    comp = comp == 3 ? STBI_rgb_alpha : comp;
    b32 use_bc = gpu->ready && !is_16_bit;
    if(use_bc){
        comp = STBI_rgb_alpha;
    }
    void *data = NULL;
    u8 size_bytes = 1;
    DOT_PROFILE_BLOCK("Bake texture decode"){
        if(is_hdr){
            data = stbi_loadf_from_memory(file_data.str, cast(int)file_data.size, &width, &height, NULL, comp);
            size_bytes = 4;
        }else if(is_16_bit){
            data = stbi_load_16_from_memory(file_data.str, cast(int)file_data.size, &width, &height, NULL, comp);
            size_bytes = 2;
        }else{
            data = stbi_load_from_memory(file_data.str, cast(int)file_data.size, &width, &height, NULL, comp);
        }
    }
    stbi_allocator_unset();
    if(!data){
        DOT_WARNING("Could not decode texture %S", path);
        temp_arena_restore(temp);
        return(false);
    }

    u8 mip_count = 1;
    for(int w = width, h = height; w > 1 || h > 1; w = DOT_MAX(w / 2, 1), h = DOT_MAX(h / 2, 1)){
        mip_count++;
    }
    DOT_ASSERT(mip_count <= DOT_MODEL_FILE_MAX_MIPS, "Texture %S has too many mips (%u)", path, mip_count);
    u8 *block = NULL;
    if(use_bc && use == DOT_ModelBakeTextureUse_Normal){
        DOT_PROFILE_BLOCK("Bake BC5"){ block = dot_model_bake_bc5(gpu, cast(u8*)data, width, height, mip_count, out); }
        if(dot_g_bake_report_bc5_quality){
            f64 psnr = dot_model_bake_bc5_psnr(block, cast(u8*)data, cast(u32)width, cast(u32)height);
            DOT_PRINT("BC5 %S: %.2f dB PSNR on mip 0", path, psnr);
        }
    }else if(use_bc){
        DOT_PROFILE_BLOCK("Bake BC7"){ block = dot_model_bake_bc7(gpu, cast(u8*)data, width, height, srgb, mip_count, out); }
    }else{
        u8 *chain = cast(u8*)data;
        if(mip_count > 1){
            DOT_PROFILE_BLOCK("Bake mip chain"){ chain = rn_mip_chain_build(temp.arena, data, width, height, comp, size_bytes, mip_count); }
        }

        out->width       = cast(u32)width;
        out->height      = cast(u32)height;
        out->format_kind = cast(u32)rb_texture_format_from_info(comp, size_bytes, srgb);
        out->mip_count   = mip_count;

        i64 texel_bytes = cast(i64)comp * size_bytes;
        u64 cursor = 0;
        for(u32 mip = 0; mip < mip_count; ++mip){
            out->mip_sizes[mip]   = cast(u64)(texel_bytes * DOT_MAX(width >> mip, 1) * DOT_MAX(height >> mip, 1));
            out->mip_offsets[mip] = dot_model_bake_place(&cursor, out->mip_sizes[mip], DOT_MODEL_FILE_ALIGN);
        }
        out->size = cursor;

        block = PUSH_ARRAY(temp.arena, u8, out->size);
        u8 *src = chain;
        for(u32 mip = 0; mip < mip_count; ++mip){
            MEMORY_COPY(block + out->mip_offsets[mip], src, out->mip_sizes[mip]);
            src += out->mip_sizes[mip];
        }
    }

    DOT_ModelFileTextureKey key = { .kind = 2, .width = out->width, .height = out->height, .format_kind = out->format_kind, .mip_count = out->mip_count };
    out->hash   = dot_hash128_from_data(&key, sizeof(key), block, out->size);
    out->offset = dot_model_bake_write_block(writer, out->hash, block, out->size);
    temp_arena_restore(temp);
    return(true);
}

internal DOT_ModelFileSampler
dot_model_bake_sampler(cgltf_sampler *sampler)
{
    DOT_ModelFileSampler result = {0};
    result.mag_filter = sampler->mag_filter == cgltf_filter_type_linear ? RB_SamplerFilterKind_Linear : RB_SamplerFilterKind_Nearest;
    switch(sampler->min_filter){
    case cgltf_filter_type_undefined:
    case cgltf_filter_type_nearest:
        result.min_filter = RB_SamplerFilterKind_Nearest;
    break;
    case cgltf_filter_type_linear:
        result.min_filter = RB_SamplerFilterKind_Linear;
    break;
    case cgltf_filter_type_linear_mipmap_nearest:
        result.min_filter    = RB_SamplerFilterKind_Linear;
        result.mipmap_filter = RB_SamplerMipmapFilterKind_Nearest;
    break;
    case cgltf_filter_type_linear_mipmap_linear:
        result.min_filter    = RB_SamplerFilterKind_Linear;
        result.mipmap_filter = RB_SamplerMipmapFilterKind_Linear;
    break;
    case cgltf_filter_type_nearest_mipmap_nearest:
        result.min_filter    = RB_SamplerFilterKind_Nearest;
        result.mipmap_filter = RB_SamplerMipmapFilterKind_Nearest;
    break;
    case cgltf_filter_type_nearest_mipmap_linear:
        result.min_filter    = RB_SamplerFilterKind_Nearest;
        result.mipmap_filter = RB_SamplerMipmapFilterKind_Linear;
    break;
    }

    switch(sampler->wrap_s){
    case cgltf_wrap_mode_clamp_to_edge  : result.address_mode_u = RB_SamplerAddressModeKind_ClampToEdge; break;
    case cgltf_wrap_mode_mirrored_repeat: result.address_mode_u = RB_SamplerAddressModeKind_MirroredRepeat; break;
    case cgltf_wrap_mode_repeat         : result.address_mode_u = RB_SamplerAddressModeKind_Repeat; break;
    }
    switch(sampler->wrap_t){
    case cgltf_wrap_mode_clamp_to_edge  : result.address_mode_v = RB_SamplerAddressModeKind_ClampToEdge; break;
    case cgltf_wrap_mode_mirrored_repeat: result.address_mode_v = RB_SamplerAddressModeKind_MirroredRepeat; break;
    case cgltf_wrap_mode_repeat         : result.address_mode_v = RB_SamplerAddressModeKind_Repeat; break;
    }
    return(result);
}

internal void
dot_model_bake_texture_view(const cgltf_data *gltf_data, b32 *texture_valid, cgltf_texture_view *view,
    DOT_ModelFileMaterial *material, DOT_PBRTextureKind kind, u32 feature_bit)
{
    if(!view->texture || !view->texture->image){
        return;
    }
    u32 texture_index = cast(u32)cgltf_image_index(gltf_data, view->texture->image);
    if(!texture_valid[texture_index]){
        return;
    }
    material->texture_indices[kind] = texture_index;
    if(view->texture->sampler){
        material->sampler_indices[kind] = cast(u32)cgltf_sampler_index(gltf_data, view->texture->sampler);
    }
    material->gpu.flags |= feature_bit;
}

internal DOT_ModelFileMaterial
dot_model_bake_material(const cgltf_data *gltf_data, b32 *texture_valid, cgltf_material *gltf_material)
{
    DOT_ModelFileMaterial material = {0};
    for EACH_INDEX(i, DOT_PBRTextureKind_Count){
        material.texture_indices[i] = DOT_MODEL_FILE_NONE;
        material.sampler_indices[i] = DOT_MODEL_FILE_NONE;
    }
    material.gpu.base_color_factor = v4(1,1,1,1);
    material.gpu.metallic_factor   = 1.0f;
    material.gpu.roughness_factor  = 1.0f;
    material.gpu.occlusion_factor  = 1.0f;
    if(!gltf_material){
        return(material);
    }

    if(gltf_material->has_pbr_metallic_roughness){
        cgltf_pbr_metallic_roughness *pbr = &gltf_material->pbr_metallic_roughness;
        material.gpu.base_color_factor = v4(pbr->base_color_factor[0], pbr->base_color_factor[1], pbr->base_color_factor[2], pbr->base_color_factor[3]);
        material.gpu.metallic_factor   = pbr->metallic_factor;
        material.gpu.roughness_factor  = pbr->roughness_factor;
        dot_model_bake_texture_view(gltf_data, texture_valid, &pbr->base_color_texture, &material, DOT_PBRTextureKind_Diffuse, DOT_MaterialFeatureBit_ColorTexture);
    }
    material.gpu.emissive_factor = v3(gltf_material->emissive_factor[0], gltf_material->emissive_factor[1], gltf_material->emissive_factor[2]);
    dot_model_bake_texture_view(gltf_data, texture_valid, &gltf_material->emissive_texture,  &material, DOT_PBRTextureKind_Emissive,  DOT_MaterialFeatureBit_EmissiveTexture);
    dot_model_bake_texture_view(gltf_data, texture_valid, &gltf_material->occlusion_texture, &material, DOT_PBRTextureKind_Occlusion, DOT_MaterialFeatureBit_OcclusionTexture);
    dot_model_bake_texture_view(gltf_data, texture_valid, &gltf_material->normal_texture,    &material, DOT_PBRTextureKind_Normal,    DOT_MaterialFeatureBit_NormalTexture);
    if(material.gpu.flags & DOT_MaterialFeatureBit_OcclusionTexture){
        material.gpu.occlusion_factor = gltf_material->occlusion_texture.scale;
    }
    return(material);
}

internal b32
dot_model_bake(RN_RenderCtx *renderer, String8 gltf_path, String8 baked_path, b32 compress)
{
    Scratch s = scratch_begin(0,0);
    cgltf_data *gltf_data = NULL;
    DOT_PROFILE_BLOCK("Bake glTF load"){ gltf_data = dot_gltf_load_from_path(s.arena, gltf_path); }
    if(!gltf_data || !gltf_data->scene){
        DOT_WARNING("Could not bake %S, failed to load the glTF or it has no default scene", gltf_path);
        scratch_end(s);
        return(false);
    }

    DOT_ModelFileHeader header = {0};
    header.magic   = DOT_MODEL_FILE_MAGIC;
    header.version = DOT_MODEL_FILE_VERSION;

    u32 *mesh_first = PUSH_ARRAY(s.arena, u32, gltf_data->meshes_count);
    for(cgltf_size m = 0; m < gltf_data->meshes_count; ++m){
        mesh_first[m] = header.mesh_count;
        header.mesh_count += cast(u32)gltf_data->meshes[m].primitives_count;
    }
    cgltf_scene *scene = gltf_data->scene;
    for(cgltf_size n = 0; n < scene->nodes_count; ++n){
        cgltf_mesh *mesh = scene->nodes[n]->mesh;
        header.submesh_count += mesh ? cast(u32)mesh->primitives_count : 0;
    }
    header.material_count = cast(u32)gltf_data->materials_count + 1;
    header.texture_count  = cast(u32)gltf_data->images_count;
    header.sampler_count  = cast(u32)gltf_data->samplers_count;

    u64 cursor = sizeof(DOT_ModelFileHeader);
    header.meshes_offset    = dot_model_bake_place(&cursor, header.mesh_count     * sizeof(DOT_ModelFileMesh),     DOT_MODEL_FILE_TABLE_ALIGN);
    header.submeshes_offset = dot_model_bake_place(&cursor, header.submesh_count  * sizeof(DOT_ModelFileSubmesh),  DOT_MODEL_FILE_TABLE_ALIGN);
    header.materials_offset = dot_model_bake_place(&cursor, header.material_count * sizeof(DOT_ModelFileMaterial), DOT_MODEL_FILE_TABLE_ALIGN);
    header.textures_offset  = dot_model_bake_place(&cursor, header.texture_count  * sizeof(DOT_ModelFileTexture),  DOT_MODEL_FILE_TABLE_ALIGN);
    header.samplers_offset  = dot_model_bake_place(&cursor, header.sampler_count  * sizeof(DOT_ModelFileSampler),  DOT_MODEL_FILE_TABLE_ALIGN);

    DOT_ModelFileMesh     *meshes    = PUSH_ARRAY(s.arena, DOT_ModelFileMesh,     header.mesh_count);
    DOT_ModelFileSubmesh  *submeshes = PUSH_ARRAY(s.arena, DOT_ModelFileSubmesh,  header.submesh_count);
    DOT_ModelFileMaterial *materials = PUSH_ARRAY(s.arena, DOT_ModelFileMaterial, header.material_count);
    DOT_ModelFileTexture  *textures  = PUSH_ARRAY(s.arena, DOT_ModelFileTexture,  header.texture_count);
    DOT_ModelFileSampler  *samplers  = PUSH_ARRAY(s.arena, DOT_ModelFileSampler,  header.sampler_count);
    b32                   *texture_valid = PUSH_ARRAY(s.arena, b32, header.texture_count);
    b32                   *mesh_valid    = PUSH_ARRAY(s.arena, b32, header.mesh_count);

    String8 tmp_path = string8_format(s.arena, "%S.tmp", baked_path);
    Scratch blocks_scratch = scratch_begin(1, &s.arena);
    DOT_ModelBakeWriter writer = {0};
    writer.file         = platform_file_open_write(tmp_path);
    writer.cursor       = cursor;
    writer.ok           = writer.file.handle != NULL;
    writer.blocks_arena = blocks_scratch.arena;
    hm_init(writer.blocks_arena, &writer.blocks, 256, hm_hasheq_bytes);

    DOT_ModelBakeTextureUse *texture_use = PUSH_ARRAY(s.arena, DOT_ModelBakeTextureUse, header.texture_count);
    for(cgltf_size i = 0; i < gltf_data->materials_count; ++i){
        cgltf_material *material = &gltf_data->materials[i];
        struct { cgltf_texture_view *view; DOT_ModelBakeTextureUse use; } views[] = {
            { &material->pbr_metallic_roughness.base_color_texture,         DOT_ModelBakeTextureUse_Color  },
            { &material->emissive_texture,                                  DOT_ModelBakeTextureUse_Color  },
            { &material->normal_texture,                                    DOT_ModelBakeTextureUse_Normal },
            { &material->pbr_metallic_roughness.metallic_roughness_texture, DOT_ModelBakeTextureUse_Data   },
            { &material->occlusion_texture,                                 DOT_ModelBakeTextureUse_Data   },
        };
        for EACH_INDEX(v, DOT_ARRAY_COUNT(views)){
            if(views[v].view->texture && views[v].view->texture->image){
                texture_use[cgltf_image_index(gltf_data, views[v].view->texture->image)] |= views[v].use;
            }
        }
    }

    DOT_ModelBakeGpu gpu = {0};
    if(compress && header.texture_count){
        dot_model_bake_gpu_init(renderer, &gpu);
    }
    String8 folder = string8_chop_last_slash(gltf_path);
    DOT_PROFILE_BLOCK("Bake textures"){
        for(u32 i = 0; writer.ok && i < header.texture_count; ++i){
            String8 image_path = string8_format(s.arena, "%S/%s", folder, gltf_data->images[i].uri);
            texture_valid[i] = dot_model_bake_texture(s.arena, &writer, &gpu, image_path, texture_use[i], &textures[i]);
        }
    }
    if(compress && header.texture_count){
        dot_model_bake_gpu_destroy(renderer, &gpu);
    }
    DOT_PROFILE_BLOCK("Bake meshes"){
        for(cgltf_size m = 0; writer.ok && m < gltf_data->meshes_count; ++m){
            cgltf_mesh *mesh = &gltf_data->meshes[m];
            for(cgltf_size p = 0; p < mesh->primitives_count; ++p){
                u32 mesh_index = mesh_first[m] + cast(u32)p;
                mesh_valid[mesh_index] = dot_model_bake_mesh(s.arena, &writer, &mesh->primitives[p], &meshes[mesh_index]);
            }
        }
    }

    for(u32 i = 0; i < header.sampler_count; ++i){
        samplers[i] = dot_model_bake_sampler(&gltf_data->samplers[i]);
    }
    for(cgltf_size i = 0; i < gltf_data->materials_count; ++i){
        materials[i] = dot_model_bake_material(gltf_data, texture_valid, &gltf_data->materials[i]);
    }
    u32 default_material = header.material_count - 1;
    materials[default_material] = dot_model_bake_material(gltf_data, texture_valid, NULL);

    u32 submesh_count = 0;
    for(cgltf_size n = 0; n < scene->nodes_count; ++n){
        cgltf_node *node = scene->nodes[n];
        if(!node->mesh){
            continue;
        }
        f32x4x4 transform;
        cgltf_node_transform_world(node, cast(cgltf_float*)&transform);
        u32 first = mesh_first[cgltf_mesh_index(gltf_data, node->mesh)];
        for(cgltf_size p = 0; p < node->mesh->primitives_count; ++p){
            if(!mesh_valid[first + p]){
                continue;
            }
            cgltf_material *material = node->mesh->primitives[p].material;
            DOT_ModelFileSubmesh *submesh = &submeshes[submesh_count++];
            submesh->transform      = transform;
            submesh->mesh_index     = first + cast(u32)p;
            submesh->material_index = material ? cast(u32)cgltf_material_index(gltf_data, material) : default_material;
        }
    }
    header.submesh_count = submesh_count;
    header.file_size     = cast(u64)writer.file.size;

    if(writer.ok){
        writer.ok = platform_file_write_at(&writer.file, cast(i64)header.meshes_offset,    meshes,    cast(i64)(header.mesh_count     * sizeof(DOT_ModelFileMesh)))
                 && platform_file_write_at(&writer.file, cast(i64)header.submeshes_offset, submeshes, cast(i64)(header.submesh_count  * sizeof(DOT_ModelFileSubmesh)))
                 && platform_file_write_at(&writer.file, cast(i64)header.materials_offset, materials, cast(i64)(header.material_count * sizeof(DOT_ModelFileMaterial)))
                 && platform_file_write_at(&writer.file, cast(i64)header.textures_offset,  textures,  cast(i64)(header.texture_count  * sizeof(DOT_ModelFileTexture)))
                 && platform_file_write_at(&writer.file, cast(i64)header.samplers_offset,  samplers,  cast(i64)(header.sampler_count  * sizeof(DOT_ModelFileSampler)))
                 && platform_file_write_at(&writer.file, 0, &header, sizeof(header));
    }
    platform_file_close(&writer.file);

    if(writer.ok){
        remove(baked_path.cstr);
        writer.ok = rename(tmp_path.cstr, baked_path.cstr) == 0;
    }
    if(!writer.ok){
        DOT_WARNING("Could not write baked model %S", baked_path);
        remove(tmp_path.cstr);
    }else{
        DOT_PRINT("Baked %S: %M, %M of duplicate blocks shared instead of written", baked_path, cast(i64)header.file_size, cast(i64)writer.deduped_bytes);
    }
    scratch_end(blocks_scratch);
    scratch_end(s);
    return(writer.ok);
}

////////////////////////////////////////////////////////////////
//
// Load (.dotm -> GPU, deduped through the asset cache)

global DOT_AssetCache g_asset_cache = {0};

internal void
dot_asset_cache_init(Arena *arena, DOT_AssetCache *cache)
{
    if(cache->initialized){
        return;
    }
    hm_init(arena, &cache->textures, 256, hm_hasheq_bytes);
    hm_init(arena, &cache->meshes,   256, hm_hasheq_bytes);
    cache->initialized = true;
}

internal void
dot_asset_cache_destroy(RN_RenderCtx *renderer, DOT_AssetCache *cache)
{
    if(!cache->initialized){
        return;
    }
    HashMapIter it = {0};
    for(RN_BindlessTexture *texture = hm_iter_next(&cache->textures, &it); texture; texture = hm_iter_next(&cache->textures, &it)){
        rn_texture_destroy(renderer, texture->handle);
    }
    MEMORY_ZERO_STRUCT(cache);
}

internal b32
dot_model_file_read_table(PlatformFile *file, Arena *arena, u64 offset, u64 size, void **out)
{
    *out = PUSH_SIZE(arena, size ? size : 1);
    return(size == 0 || platform_file_read_at(file, cast(i64)offset, *out, cast(i64)size));
}

internal DOT_Model
dot_model_load_baked(RN_RenderCtx *renderer, DOT_AssetCache *cache, String8 baked_path)
{
    DOT_Model model = {0};
    dot_asset_cache_init(renderer->permanent_arena, cache);

    PlatformFile file = platform_file_open_read(baked_path);
    if(!file.handle){
        return(model);
    }
    DOT_ModelFileHeader header = {0};
    if(!platform_file_read_at(&file, 0, &header, sizeof(header)) || header.magic != DOT_MODEL_FILE_MAGIC ||
       header.version != DOT_MODEL_FILE_VERSION || header.file_size != cast(u64)file.size){
        DOT_WARNING("Baked model %S is invalid or out of date", baked_path);
        platform_file_close(&file);
        return(model);
    }

    Scratch s = scratch_begin(0,0);
    DOT_ModelFileMesh     *meshes    = NULL;
    DOT_ModelFileSubmesh  *submeshes = NULL;
    DOT_ModelFileMaterial *materials = NULL;
    DOT_ModelFileTexture  *textures  = NULL;
    DOT_ModelFileSampler  *samplers  = NULL;
    b32 tables_ok = dot_model_file_read_table(&file, s.arena, header.meshes_offset,    header.mesh_count     * sizeof(DOT_ModelFileMesh),     cast(void**)&meshes)
                 && dot_model_file_read_table(&file, s.arena, header.submeshes_offset, header.submesh_count  * sizeof(DOT_ModelFileSubmesh),  cast(void**)&submeshes)
                 && dot_model_file_read_table(&file, s.arena, header.materials_offset, header.material_count * sizeof(DOT_ModelFileMaterial), cast(void**)&materials)
                 && dot_model_file_read_table(&file, s.arena, header.textures_offset,  header.texture_count  * sizeof(DOT_ModelFileTexture),  cast(void**)&textures)
                 && dot_model_file_read_table(&file, s.arena, header.samplers_offset,  header.sampler_count  * sizeof(DOT_ModelFileSampler),  cast(void**)&samplers);
    if(!tables_ok){
        DOT_WARNING("Could not read the tables of %S", baked_path);
        platform_file_close(&file);
        scratch_end(s);
        return(model);
    }

    u32 *texture_slots = PUSH_ARRAY(s.arena, u32, header.texture_count);
    u32 *sampler_slots = PUSH_ARRAY(s.arena, u32, header.sampler_count);
    RB_GpuAddress *mesh_addresses = PUSH_ARRAY(s.arena, RB_GpuAddress, header.mesh_count);
    u32 textures_uploaded = 0;
    u32 meshes_uploaded   = 0;

    rn_upload_begin(renderer);

    DOT_PROFILE_BLOCK("Baked textures"){
        for(u32 i = 0; i < header.texture_count; ++i){
            DOT_ModelFileTexture *entry = &textures[i];
            if(!entry->mip_count){
                continue;
            }
            RN_BindlessTexture *cached = hm_get(&cache->textures, entry->hash);
            if(!cached){
                RB_TextureDesc desc = {0};
                desc.dimension_kind = RB_TextureDimensionKind_2D;
                desc.format_kind    = cast(RB_FormatKind)entry->format_kind;
                desc.mip_levels     = cast(u8)entry->mip_count;
                desc.width          = cast(u16)entry->width;
                desc.height         = cast(u16)entry->height;
                desc.depth          = 1;
                desc.array_layers   = 1;
                desc.debug_name     = string8_format(s.arena, "tex %08" PRIx64, entry->hash.lo & 0xffffffffull);
                RN_BindlessTexture texture = rn_texture_create_bindless_in_upload(renderer, &desc);

                RB_CpuGpuSpan staging = rn_upload_reserve(renderer, cast(i64)entry->size);
                if(platform_file_read_at(&file, cast(i64)entry->offset, staging.cpu, cast(i64)entry->size)){
                    for(u32 mip = 0; mip < entry->mip_count; ++mip){
                        RB_GpuRange mip_range = { staging.gpu.gpu + entry->mip_offsets[mip], cast(i64)entry->mip_sizes[mip] };
                        rn_upload_copy_texture(renderer, mip_range, texture.handle, (RB_TextureCopyDesc){ .mip_level = mip });
                    }
                }else{
                    DOT_WARNING("Could not read texture %u of %S", i, baked_path);
                }
                cached = hm_get_or_create(renderer->permanent_arena, &cache->textures, entry->hash);
                *cached = texture;
                textures_uploaded += 1;
            }
            texture_slots[i] = cached->bindless_slot;
        }
    }

    for(u32 i = 0; i < header.sampler_count; ++i){
        DOT_ModelFileSampler *entry = &samplers[i];
        RB_SamplerDesc desc = {
            .debug_name     = str_comp("Baked"),
            .min_filter     = cast(RB_SamplerFilterKind)entry->min_filter,
            .mag_filter     = cast(RB_SamplerFilterKind)entry->mag_filter,
            .mipmap_filter  = cast(RB_SamplerMipmapFilterKind)entry->mipmap_filter,
            .address_mode_u = cast(RB_SamplerAddressModeKind)entry->address_mode_u,
            .address_mode_v = cast(RB_SamplerAddressModeKind)entry->address_mode_v,
        };
        desc.max_anisotropy = dot_model_sampler_anisotropy(&desc);
        sampler_slots[i] = rn_sampler_bindless_slot_create(renderer, &desc);
    }

    DOT_PROFILE_BLOCK("Baked meshes"){
        for(u32 i = 0; i < header.mesh_count; ++i){
            DOT_ModelFileMesh *entry = &meshes[i];
            if(!entry->size){
                continue;
            }
            RB_GpuAddress *cached = hm_get(&cache->meshes, entry->hash);
            if(!cached){
                RB_GpuAddress address = rn_data_alloc(renderer, cast(i64)entry->size);
                RB_CpuGpuSpan staging = rn_upload_reserve(renderer, cast(i64)entry->size);
                if(platform_file_read_at(&file, cast(i64)entry->offset, staging.cpu, cast(i64)entry->size)){
                    rn_upload_copy_memory(renderer, staging.gpu, address);
                }else{
                    DOT_WARNING("Could not read mesh %u of %S", i, baked_path);
                }
                cached = hm_get_or_create(renderer->permanent_arena, &cache->meshes, entry->hash);
                *cached = address;
                meshes_uploaded += 1;
            }
            mesh_addresses[i] = *cached;
        }
    }

    DOT_MaterialGpu *material_gpus = PUSH_ARRAY(s.arena, DOT_MaterialGpu, header.material_count);
    for(u32 i = 0; i < header.material_count; ++i){
        DOT_ModelFileMaterial *entry = &materials[i];
        material_gpus[i] = entry->gpu;
        for EACH_INDEX(k, DOT_PBRTextureKind_Count){
            u32 texture_index = entry->texture_indices[k];
            u32 sampler_index = entry->sampler_indices[k];
            material_gpus[i].tex_slots[k]     = texture_index != DOT_MODEL_FILE_NONE ? texture_slots[texture_index] : 0;
            material_gpus[i].sampler_slots[k] = sampler_index != DOT_MODEL_FILE_NONE ? sampler_slots[sampler_index] : 0;
        }
    }
    i64 materials_size = cast(i64)(header.material_count * sizeof(DOT_MaterialGpu));
    RB_GpuAddress materials_address = rn_data_alloc(renderer, materials_size);
    rn_upload_memory(renderer, materials_address, material_gpus, materials_size);

    model.sub_mesh_draw_count = header.submesh_count;
    model.sub_mesh_draw = PUSH_ARRAY(renderer->permanent_arena, DOT_Submesh, header.submesh_count);
    for(u32 i = 0; i < header.submesh_count; ++i){
        DOT_ModelFileSubmesh *entry = &submeshes[i];
        DOT_ModelFileMesh *mesh = &meshes[entry->mesh_index];
        RB_GpuAddress base = mesh_addresses[entry->mesh_index];
        DOT_Submesh *sub_mesh = &model.sub_mesh_draw[i];
        sub_mesh->transform     = entry->transform;
        sub_mesh->index_count   = mesh->index_count;
        sub_mesh->index_addr    = base + mesh->index_offset;
        sub_mesh->position_addr = base + mesh->position_offset;
        sub_mesh->normal_addr   = base + mesh->normal_offset;
        sub_mesh->texcoord_addr = base + mesh->texcoord_offset;
        sub_mesh->tangent_addr  = mesh->has_tangents ? base + mesh->tangent_offset : 0;
        sub_mesh->material_addr = materials_address + entry->material_index * sizeof(DOT_MaterialGpu);
    }

    DOT_PROFILE_BLOCK("Baked upload submit"){ rn_upload_end(renderer); }

    DOT_PRINT("Loaded %S: %u/%u textures and %u/%u meshes uploaded, the rest reused from the cache",
        baked_path, textures_uploaded, header.texture_count, meshes_uploaded, header.mesh_count);
    platform_file_close(&file);
    scratch_end(s);
    return(model);
}

internal DOT_Model
dot_model_load(RN_RenderCtx *renderer, String8 gltf_path)
{
    Scratch s = scratch_begin(0,0);
    String8 baked_path = dot_model_baked_path(s.arena, gltf_path);
    os_directory_create_if_not_exists(str_comp("build"));
    os_directory_create_if_not_exists(str_comp(DOT_MODEL_CACHE_PATH));

    b32 baked = !platform_file_is_newer(gltf_path, baked_path);
    if(baked){
        PlatformFile file = platform_file_open_read(baked_path);
        DOT_ModelFileHeader header = {0};
        baked = file.handle && platform_file_read_at(&file, 0, &header, sizeof(header)) &&
                header.magic == DOT_MODEL_FILE_MAGIC && header.version == DOT_MODEL_FILE_VERSION;
        platform_file_close(&file);
    }
    if(!baked){
        DOT_PRINT("Baking %S into %S", gltf_path, baked_path);
        DOT_PROFILE_BLOCK("Model bake"){ baked = dot_model_bake(renderer, gltf_path, baked_path, rb_device_info().texture_compression_bc); }
    }

    DOT_Model model = {0};
    if(baked){
        DOT_PROFILE_BLOCK("Baked model load"){ model = dot_model_load_baked(renderer, &g_asset_cache, baked_path); }
    }
    scratch_end(s);
    return(model);
}
