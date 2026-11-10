////////////////////////////////////////////////////////////////
/// DOT_MODEL_BAKE_SETTINGS

DOT_SETTING(b32,"Bake", dot_g_bake_bc7_three_subsets, false);
DOT_SETTING(b32,"Bake", dot_g_bake_report_bc5_quality, false);

////////////////////////////////////////////////////////////////
/// DOT_MODEL_BAKE_CONSTANTS

enum
{
    DOT_BC_BLOCK_DIM                  = 4,
    DOT_BC_BLOCK_PIXELS               = DOT_BC_BLOCK_DIM * DOT_BC_BLOCK_DIM,
    DOT_BC_BLOCK_BYTES                = 16,
    DOT_BC4_BLOCK_BYTES               = 8,
    DOT_BC4_ENDPOINT_BYTES            = 2,
    DOT_BC4_INDEX_BYTES               = DOT_BC4_BLOCK_BYTES - DOT_BC4_ENDPOINT_BYTES,
    DOT_BC4_INDEX_BITS                = 3,
    DOT_BC4_INDEX_MASK                = (1 << DOT_BC4_INDEX_BITS) - 1,
    DOT_BC4_PALETTE_SIZE              = 8,
    DOT_BC5_CHANNELS                  = 2,
    DOT_RGBA8_CHANNELS                = 4,

    DOT_TEXTURE_BAKE_BC7_GROUP_BLOCKS = 4,
    DOT_TEXTURE_BAKE_BC7_BATCH_BLOCKS = 65532,
    DOT_TEXTURE_BAKE_BC5_GROUP_BLOCKS = 64,
    DOT_TEXTURE_BAKE_BC5_BATCH_BLOCKS = 65535 * DOT_TEXTURE_BAKE_BC5_GROUP_BLOCKS,
};

////////////////////////////////////////////////////////////////
/// DOT_MODEL_BAKE_TYPES

// Hashed in front of a block's bytes, so identical bytes with a different layout never dedupe
typedef struct DOT_ModelFileMeshKey
{
    u32 kind;
    u32 index_count;
    u32 vertex_count;
    u32 has_tangents;
}DOT_ModelFileMeshKey;

typedef struct DOT_ModelFileTextureKey
{
    u32 kind;
    u32 width;
    u32 height;
    u32 format_kind;
    u32 mip_count;
}DOT_ModelFileTextureKey;

// The header and tables of one .dotm, either read from the file or filled in by a bake
typedef struct DOT_ModelFileTables
{
    DOT_ModelFileHeader    header;
    DOT_ModelFileMesh     *meshes;
    DOT_ModelFileSubmesh  *submeshes;
    DOT_ModelFileMaterial *materials;
    DOT_ModelFileTexture  *textures;
    DOT_ModelFileSampler  *samplers;
}DOT_ModelFileTables;

// How materials sample a texture: Color picks sRGB, Normal-only picks BC5, everything else BC7
typedef u32 DOT_ModelBakeTextureUse;
enum
{
    DOT_ModelBakeTextureUse_Color  = 1 << 0,
    DOT_ModelBakeTextureUse_Normal = 1 << 1,
    DOT_ModelBakeTextureUse_Data   = 1 << 2,
};

// Where each unique block was written in the .dotm, keyed by its hash. Open addressing with a
// capacity fixed before baking, so it never allocates inside the bake's temp regions
typedef struct DOT_BlockTableEntry
{
    Hash128 hash;
    u64     file_offset;
    b32     occupied;
}DOT_BlockTableEntry;

typedef struct DOT_BlockTable
{
    DOT_BlockTableEntry *entries;
    u64                  capacity;
    u64                  deduped_bytes;
}DOT_BlockTable;

// Compute pipelines for baking textures: mip generation and the BC7/BC5 encoders. The memory
// they work in comes from the renderer's upload, frame, scratch and readback heaps
typedef struct DOT_TextureBakePipelines
{
    b32         ready;
    RB_Pipeline bc7_try_mode_456;
    RB_Pipeline bc7_try_mode_137;
    RB_Pipeline bc7_try_mode_02;
    RB_Pipeline bc7_encode_block;
    RB_Pipeline bc5_encode;
    RB_Pipeline mip_downsample;
}DOT_TextureBakePipelines;

// One texture's mip chain as the encoders see it: per-mip sizes and block counts, where each
// mip's pixels live in GPU scratch, and the readback span the encoded blocks land in
typedef struct DOT_TextureBakeLayout
{
    u32           mip_width[DOT_MODEL_FILE_MAX_MIPS];
    u32           mip_height[DOT_MODEL_FILE_MAX_MIPS];
    u32           mip_blocks_x[DOT_MODEL_FILE_MAX_MIPS];
    u32           mip_blocks[DOT_MODEL_FILE_MAX_MIPS];
    u64           mip_first_block[DOT_MODEL_FILE_MAX_MIPS];
    RB_GpuAddress mip_pixels[DOT_MODEL_FILE_MAX_MIPS];
    u64           total_blocks;
    i64           pixels_size;
    RB_CpuGpuSpan readback;
}DOT_TextureBakeLayout;

// One pass of the BC7 encoder: which entry point, which mode it tries, and how many blocks one
// thread group covers
typedef struct DOT_TextureBakeBC7Pass
{
    RB_Pipeline pipeline;
    u32         mode_id;
    u32         blocks_per_group;
}DOT_TextureBakeBC7Pass;

// A texture whose encode is recorded in the upload batch but not read back yet: its file table entry
// and the readback memory its encoded blocks land in once the batch is flushed
typedef struct DOT_TextureBakePending
{
    DOT_ModelFileTexture *out;
    u8                   *blocks;
}DOT_TextureBakePending;

// Everything one model bake works with: the .dotm being written, where the next block goes, the
// blocks already in it, the textures waiting on the GPU, and the cache that receives each GPU
// resource as it's baked
typedef struct DOT_ModelBakeCtx
{
    Arena                    *arena;
    DOT_AssetCache           *cache;
    DOT_TextureBakePipelines  textures;
    DOT_TextureBakePending   *pending;
    u32                       pending_count;
    PlatformFile              file;
    b32                       file_ok;
    u64                       file_cursor;
    DOT_BlockTable            blocks;
}DOT_ModelBakeCtx;

////////////////////////////////////////////////////////////////
/// DOT_MODEL_GLOBALS

global DOT_AssetCache g_asset_cache = {0};

////////////////////////////////////////////////////////////////
/// DOT_MODEL_PRIVATE_DECLARATIONS

internal DOT_Model dot_model_from_tables(DOT_AssetCache *cache, DOT_ModelFileTables *tables);

////////////////////////////////////////////////////////////////
/// DOT_GLTF_FUNCTIONS

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
dot_model_from_gltf(DOT_AssetCache *cache, const cgltf_data *gltf_data, String8 gltf_path)
{
    RN_RenderCtx *renderer = cache->renderer;
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

    DOT_PROFILE_BLOCK("Geometry and materials"){
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
    DOT_PROFILE_BLOCK("glTF parse"){ 
        result = cgltf_parse(&options, buff.str, cast(u64)buff.size, &gltf_data); 
    }
    b32 fail_parse = false;
    if(result != cgltf_result_success){
        fail_parse = true;
        DOT_WARNING("cgltf_parse failed: %d", result);
    }
    b32 load_buffers_failed = false;
    if(!fail_parse){
        DOT_PROFILE_BLOCK("glTF load buffers"){ 
            result = cgltf_load_buffers(&options, gltf_data, path.cstr); 
        }
        load_buffers_failed = result != cgltf_result_success;
    }
    if(load_buffers_failed){
        DOT_WARNING("cgltf_load_buffers failed: %d", result);
        gltf_data = NULL;
    }
	return(gltf_data);
}

internal DOT_Model
dot_model_load_from_path(DOT_AssetCache *cache, String8 path)
{
    Scratch s = scratch_begin(0,0);
    cgltf_data *gltf = dot_gltf_load_from_path(s.arena, path);
    DOT_Model model = {0};
    if(gltf){
        model = dot_model_from_gltf(cache, gltf, path);
        // cgltf_free(gltf);
    }
    scratch_end(s);
	return(model);
}

////////////////////////////////////////////////////////////////
/// DOT_MODEL_BAKE_FILE_FUNCTIONS

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

internal DOT_BlockTable
dot_block_table_create(Arena *arena, u64 max_blocks)
{
    DOT_BlockTable table = {0};
    table.capacity = 1;
    while(table.capacity < 2 * max_blocks + 1){
        table.capacity <<= 1;
    }
    table.entries = PUSH_ARRAY(arena, DOT_BlockTableEntry, table.capacity);
    return(table);
}

internal u64
dot_model_bake_push_block(DOT_ModelBakeCtx *ctx, Hash128 hash, void const *data, u64 size)
{
    DOT_BlockTable *table = &ctx->blocks;
    u64 slot = hash.lo & (table->capacity - 1);
    while(table->entries[slot].occupied){
        DOT_BlockTableEntry *entry = &table->entries[slot];
        if(entry->hash.lo == hash.lo && entry->hash.hi == hash.hi){
            table->deduped_bytes += size;
            return(entry->file_offset);
        }
        slot = (slot + 1) & (table->capacity - 1);
    }
    u64 file_offset = dot_model_bake_place(&ctx->file_cursor, size, DOT_MODEL_FILE_ALIGN);
    ctx->file_ok = ctx->file_ok && platform_file_write_at(&ctx->file, cast(i64)file_offset, data, cast(i64)size);
    table->entries[slot] = (DOT_BlockTableEntry){ .hash = hash, .file_offset = file_offset, .occupied = true };
    return(file_offset);
}

////////////////////////////////////////////////////////////////
/// DOT_MODEL_BAKE_MESH_FUNCTIONS

internal b32
dot_model_bake_mesh(DOT_ModelBakeCtx *ctx, cgltf_primitive *primitive, DOT_ModelFileMesh *out)
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

    TempArena temp = temp_arena_start(ctx->arena);
    b32 has_tangents  = tangent_accessor != NULL;
    out->index_count  = cast(u32)index_accessor->count;
    out->vertex_count = cast(u32)position_accessor->count;

    u64 cursor = 0;
    out->index_offset    = dot_model_bake_place(&cursor, out->index_count  * sizeof(u32),   DOT_MODEL_FILE_ALIGN);
    out->position_offset = dot_model_bake_place(&cursor, out->vertex_count * sizeof(f32x3), DOT_MODEL_FILE_ALIGN);
    out->normal_offset   = dot_model_bake_place(&cursor, out->vertex_count * sizeof(f32x3), DOT_MODEL_FILE_ALIGN);
    out->texcoord_offset = dot_model_bake_place(&cursor, out->vertex_count * sizeof(f32x2), DOT_MODEL_FILE_ALIGN);
    if(has_tangents){
        out->tangent_offset = dot_model_bake_place(&cursor, out->vertex_count * sizeof(f32x4), DOT_MODEL_FILE_ALIGN);
    }
    out->size = cursor;

    u8 *block = PUSH_ARRAY(temp.arena, u8, out->size);
    cgltf_accessor_unpack_indices(index_accessor, block + out->index_offset, sizeof(u32), out->index_count);
    cgltf_accessor_unpack_floats(position_accessor, cast(f32*)(block + out->position_offset), out->vertex_count * 3);
    cgltf_accessor_unpack_floats(normal_accessor,   cast(f32*)(block + out->normal_offset),   out->vertex_count * 3);
    cgltf_accessor_unpack_floats(texcoord_accessor, cast(f32*)(block + out->texcoord_offset), out->vertex_count * 2);
    if(has_tangents){
        cgltf_accessor_unpack_floats(tangent_accessor, cast(f32*)(block + out->tangent_offset), out->vertex_count * 4);
    }

    DOT_ModelFileMeshKey key = { .kind = 1, .index_count = out->index_count, .vertex_count = out->vertex_count, .has_tangents = has_tangents };
    out->hash   = hash_xxh3_128(block, out->size, hash_xxh3_u64(&key, sizeof(key)));
    out->offset = dot_model_bake_push_block(ctx, out->hash, block, out->size);
    if(!hm_get(&ctx->cache->meshes, out->hash)){
        RB_CpuGpuSpan staging = rn_upload_reserve(ctx->cache->renderer, cast(i64)out->size);
        MEMORY_COPY(staging.cpu, block, out->size);
        dot_asset_cache_add_mesh(ctx->cache, out, staging);
    }
    temp_arena_restore(temp);
    return(true);
}

////////////////////////////////////////////////////////////////
/// DOT_TEXTURE_BAKE_FUNCTIONS

internal b32
dot_texture_bake_pipelines_init(RN_RenderCtx *renderer, DOT_TextureBakePipelines *pipelines)
{
    String8 bc7_path = str_comp(RN_SHADER_PATH "bc7_encode.comp");
    pipelines->bc7_try_mode_456 = rn_compute_pipeline_create(renderer, bc7_path, str_comp("TryMode456CS"));
    pipelines->bc7_try_mode_137 = rn_compute_pipeline_create(renderer, bc7_path, str_comp("TryMode137CS"));
    pipelines->bc7_try_mode_02  = rn_compute_pipeline_create(renderer, bc7_path, str_comp("TryMode02CS"));
    pipelines->bc7_encode_block = rn_compute_pipeline_create(renderer, bc7_path, str_comp("EncodeBlockCS"));
    pipelines->bc5_encode       = rn_compute_pipeline_create(renderer, str_comp(RN_SHADER_PATH "bc5_encode.comp"), (String8){0});
    pipelines->mip_downsample   = rn_compute_pipeline_create(renderer, str_comp(RN_SHADER_PATH "mip_downsample.comp"), (String8){0});
    pipelines->ready = RB_HANDLE_VALID(pipelines->bc7_try_mode_456) && RB_HANDLE_VALID(pipelines->bc7_try_mode_137)
                    && RB_HANDLE_VALID(pipelines->bc7_try_mode_02)  && RB_HANDLE_VALID(pipelines->bc7_encode_block)
                    && RB_HANDLE_VALID(pipelines->bc5_encode)       && RB_HANDLE_VALID(pipelines->mip_downsample);
    if(!pipelines->ready){
        DOT_WARNING("Could not create the BC encode pipelines, baking textures uncompressed");
    }
    return(pipelines->ready);
}

internal void
dot_texture_bake_pipelines_end(RN_RenderCtx *renderer, DOT_TextureBakePipelines *pipelines)
{
    RB_Pipeline handles[] = { pipelines->bc7_try_mode_456, pipelines->bc7_try_mode_137, pipelines->bc7_try_mode_02, pipelines->bc7_encode_block, pipelines->bc5_encode, pipelines->mip_downsample };
    for EACH_INDEX(i, DOT_ARRAY_COUNT(handles)){
        if(RB_HANDLE_VALID(handles[i])){
            rn_pipeline_destroy(renderer, handles[i]);
        }
    }
}

internal void
dot_texture_bake_layout_compute(DOT_TextureBakeLayout *layout, int width, int height, u8 mip_count, DOT_ModelFileTexture *out)
{
    out->width     = cast(u32)width;
    out->height    = cast(u32)height;
    out->mip_count = mip_count;
    u64 cursor = 0;
    for(u32 mip = 0; mip < mip_count; ++mip){
        layout->mip_width[mip]       = cast(u32)DOT_MAX(width  >> mip, 1);
        layout->mip_height[mip]      = cast(u32)DOT_MAX(height >> mip, 1);
        layout->mip_blocks_x[mip]    = (layout->mip_width[mip] + 3) / 4;
        layout->mip_blocks[mip]      = layout->mip_blocks_x[mip] * ((layout->mip_height[mip] + 3) / 4);
        layout->mip_first_block[mip] = layout->total_blocks;
        layout->total_blocks  += layout->mip_blocks[mip];
        layout->pixels_size   += ALIGN_POW2(cast(i64)layout->mip_width[mip] * layout->mip_height[mip] * DOT_RGBA8_CHANNELS, cast(i64)RB_GPU_ARENA_ALIGN);
        out->mip_sizes[mip]   = cast(u64)layout->mip_blocks[mip] * DOT_BC_BLOCK_BYTES;
        out->mip_offsets[mip] = dot_model_bake_place(&cursor, out->mip_sizes[mip], DOT_MODEL_FILE_ALIGN);
    }
    out->size = cursor;
}

internal i64
dot_texture_bake_bc7_candidates_size(DOT_TextureBakeLayout *layout)
{
    return(cast(i64)(layout->total_blocks + DOT_TEXTURE_BAKE_BC7_GROUP_BLOCKS) * DOT_BC_BLOCK_BYTES);
}

internal void
dot_texture_bake_upload(RN_RenderCtx *renderer, DOT_TextureBakeLayout *layout, u8 *rgba, u8 mip_count, u64 readback_size)
{
    for(u32 mip = 0; mip < mip_count; ++mip){
        i64 level_size = cast(i64)layout->mip_width[mip] * layout->mip_height[mip] * DOT_RGBA8_CHANNELS;
        layout->mip_pixels[mip] = rn_scratch_alloc(renderer, level_size);
    }
    i64 mip0_size = cast(i64)layout->mip_width[0] * layout->mip_height[0] * DOT_RGBA8_CHANNELS;
    rn_upload_memory(renderer, layout->mip_pixels[0], rgba, mip0_size);
    rb_barrier(renderer->upload_cmd, RB_StageBit_Transfer, RB_AccessBit_TransferWrite, RB_StageBit_Compute, RB_AccessBit_ShaderRead);

    layout->readback = rn_readback_reserve(renderer, cast(i64)readback_size);
    MEMORY_ZERO(layout->readback.cpu, readback_size);
}

internal void
dot_texture_bake_downsample(RN_RenderCtx *renderer, DOT_TextureBakePipelines *pipelines, DOT_TextureBakeLayout *layout, b32 srgb, u8 mip_count)
{
    if(mip_count <= 1){
        return;
    }
    RB_CommandBuffer cmd = renderer->upload_cmd;
    rb_bind_pso(cmd, pipelines->mip_downsample);
    for(u32 mip = 1; mip < mip_count; ++mip){
        RN_MipDownsampleRoot root = {0};
        root.src        = layout->mip_pixels[mip - 1];
        root.dst        = layout->mip_pixels[mip];
        root.src_width  = layout->mip_width[mip - 1];
        root.src_height = layout->mip_height[mip - 1];
        root.dst_width  = layout->mip_width[mip];
        root.dst_height = layout->mip_height[mip];
        root.srgb       = srgb;
        u32 groups_x = (root.dst_width  + RN_MIP_DOWNSAMPLE_GROUP_DIM - 1) / RN_MIP_DOWNSAMPLE_GROUP_DIM;
        u32 groups_y = (root.dst_height + RN_MIP_DOWNSAMPLE_GROUP_DIM - 1) / RN_MIP_DOWNSAMPLE_GROUP_DIM;
        rb_dispatch(cmd, rn_frame_push(renderer, root).gpu, groups_x, groups_y, 1);
        rb_barrier(cmd, RB_StageBit_Compute, RB_AccessBit_ShaderWrite, RB_StageBit_Compute, RB_AccessBit_ShaderRead);
    }
}

internal u8 *
dot_texture_bake_bc7(RN_RenderCtx *renderer, DOT_TextureBakePipelines *pipelines, DOT_TextureBakeLayout *layout, u8 *rgba, b32 srgb, DOT_ModelFileTexture *out)
{
    u8 mip_count     = cast(u8)out->mip_count;
    out->format_kind = srgb ? RB_FormatKind_Bc7_Srgb : RB_FormatKind_Bc7_Unorm;

    DOT_TextureBakeBC7Pass passes[7];
    u32 pass_count = 0;
    passes[pass_count++] = (DOT_TextureBakeBC7Pass){ pipelines->bc7_try_mode_456, 0, DOT_TEXTURE_BAKE_BC7_GROUP_BLOCKS };
    passes[pass_count++] = (DOT_TextureBakeBC7Pass){ pipelines->bc7_try_mode_137, 1, 1 };
    passes[pass_count++] = (DOT_TextureBakeBC7Pass){ pipelines->bc7_try_mode_137, 3, 1 };
    passes[pass_count++] = (DOT_TextureBakeBC7Pass){ pipelines->bc7_try_mode_137, 7, 1 };
    if(dot_g_bake_bc7_three_subsets){
        passes[pass_count++] = (DOT_TextureBakeBC7Pass){ pipelines->bc7_try_mode_02, 0, 1 };
        passes[pass_count++] = (DOT_TextureBakeBC7Pass){ pipelines->bc7_try_mode_02, 2, 1 };
    }
    passes[pass_count++] = (DOT_TextureBakeBC7Pass){ pipelines->bc7_encode_block, 0, DOT_TEXTURE_BAKE_BC7_GROUP_BLOCKS };

    dot_texture_bake_upload(renderer, layout, rgba, mip_count, out->size);
    RB_GpuAddress candidates = rn_scratch_alloc(renderer, dot_texture_bake_bc7_candidates_size(layout));

    dot_texture_bake_downsample(renderer, pipelines, layout, srgb, mip_count);
    RB_CommandBuffer cmd = renderer->upload_cmd;
    for(u32 p = 0; p < pass_count; ++p){
        DOT_TextureBakeBC7Pass pass = passes[p];
        b32 last = p == pass_count - 1;
        rb_bind_pso(cmd, pass.pipeline);
        for(u32 mip = 0; mip < mip_count; ++mip){
            RB_GpuAddress mip_candidates = candidates + layout->mip_first_block[mip] * DOT_BC_BLOCK_BYTES;
            for(u32 start = 0; start < layout->mip_blocks[mip]; start += DOT_TEXTURE_BAKE_BC7_BATCH_BLOCKS){
                u32 batch_blocks = DOT_MIN(layout->mip_blocks[mip] - start, DOT_TEXTURE_BAKE_BC7_BATCH_BLOCKS);
                RN_BC7EncodeRoot root = {0};
                root.pixels           = layout->mip_pixels[mip];
                root.in_buffer        = mip_candidates;
                root.out_buffer       = last ? layout->readback.gpu.gpu + out->mip_offsets[mip] : mip_candidates;
                root.tex_width        = layout->mip_width[mip];
                root.tex_height       = layout->mip_height[mip];
                root.num_block_x      = layout->mip_blocks_x[mip];
                root.mode_id          = pass.mode_id;
                root.start_block_id   = start;
                root.num_total_blocks = layout->mip_blocks[mip];
                root.alpha_weight     = 1.0f;
                rb_dispatch(cmd, rn_frame_push(renderer, root).gpu, (batch_blocks + pass.blocks_per_group - 1) / pass.blocks_per_group, 1, 1);
            }
        }
        if(last){
            rb_barrier(cmd, RB_StageBit_Compute, RB_AccessBit_ShaderWrite, RB_StageBit_Host, RB_AccessBit_HostRead);
        }else{
            rb_barrier(cmd, RB_StageBit_Compute, RB_AccessBit_ShaderWrite, RB_StageBit_Compute, RB_AccessBit_ShaderRead | RB_AccessBit_ShaderWrite);
        }
    }
    return(cast(u8*)layout->readback.cpu);
}

internal u8 *
dot_texture_bake_bc5(RN_RenderCtx *renderer, DOT_TextureBakePipelines *pipelines, DOT_TextureBakeLayout *layout, u8 *rgba, DOT_ModelFileTexture *out)
{
    u8 mip_count     = cast(u8)out->mip_count;
    out->format_kind = RB_FormatKind_Bc5_Rg;

    dot_texture_bake_upload(renderer, layout, rgba, mip_count, out->size);
    dot_texture_bake_downsample(renderer, pipelines, layout, false, mip_count);
    RB_CommandBuffer cmd = renderer->upload_cmd;
    rb_bind_pso(cmd, pipelines->bc5_encode);
    for(u32 mip = 0; mip < mip_count; ++mip){
        for(u32 start = 0; start < layout->mip_blocks[mip]; start += DOT_TEXTURE_BAKE_BC5_BATCH_BLOCKS){
            u32 batch_blocks = DOT_MIN(layout->mip_blocks[mip] - start, DOT_TEXTURE_BAKE_BC5_BATCH_BLOCKS);
            RN_BC5EncodeRoot root = {0};
            root.pixels           = layout->mip_pixels[mip];
            root.out_buffer       = layout->readback.gpu.gpu + out->mip_offsets[mip];
            root.tex_width        = layout->mip_width[mip];
            root.tex_height       = layout->mip_height[mip];
            root.num_block_x      = layout->mip_blocks_x[mip];
            root.start_block_id   = start;
            root.num_total_blocks = layout->mip_blocks[mip];
            rb_dispatch(cmd, rn_frame_push(renderer, root).gpu, (batch_blocks + DOT_TEXTURE_BAKE_BC5_GROUP_BLOCKS - 1) / DOT_TEXTURE_BAKE_BC5_GROUP_BLOCKS, 1, 1);
        }
    }
    rb_barrier(cmd, RB_StageBit_Compute, RB_AccessBit_ShaderWrite, RB_StageBit_Host, RB_AccessBit_HostRead);
    return(cast(u8*)layout->readback.cpu);
}

////////////////////////////////////////////////////////////////
/// DOT_BC_QUALITY_FUNCTIONS

internal void
dot_bc4_palette(u8 const *bc4_block, u32 palette[DOT_BC4_PALETTE_SIZE])
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
dot_bc4_decode_block(u8 const *bc4_block, u8 pixels[DOT_BC_BLOCK_PIXELS])
{
    u32 palette[DOT_BC4_PALETTE_SIZE];
    dot_bc4_palette(bc4_block, palette);
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
dot_bc5_psnr(u8 const *bc5_blocks, u8 const *rgba, u32 width, u32 height)
{
    u32 blocks_x = (width  + DOT_BC_BLOCK_DIM - 1) / DOT_BC_BLOCK_DIM;
    u32 blocks_y = (height + DOT_BC_BLOCK_DIM - 1) / DOT_BC_BLOCK_DIM;
    f64 squared_error_sum = 0.0;
    u64 sample_count      = 0;
    for(u32 block_y = 0; block_y < blocks_y; ++block_y){
        for(u32 block_x = 0; block_x < blocks_x; ++block_x){
            u8 const *bc5_block = bc5_blocks + (cast(u64)block_y * blocks_x + block_x) * DOT_BC_BLOCK_BYTES;
            for(u32 channel = 0; channel < DOT_BC5_CHANNELS; ++channel){
                u8 decoded[DOT_BC_BLOCK_PIXELS];
                dot_bc4_decode_block(bc5_block + channel * DOT_BC4_BLOCK_BYTES, decoded);
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

////////////////////////////////////////////////////////////////
/// DOT_MODEL_BAKE_TEXTURE_FUNCTIONS

internal void
dot_model_bake_texture_finish(DOT_ModelBakeCtx *ctx, DOT_ModelFileTexture *out, u8 *blocks)
{
    DOT_ModelFileTextureKey key = { .kind = 2, .width = out->width, .height = out->height, .format_kind = out->format_kind, .mip_count = out->mip_count };
    out->hash   = hash_xxh3_128(blocks, out->size, hash_xxh3_u64(&key, sizeof(key)));
    out->offset = dot_model_bake_push_block(ctx, out->hash, blocks, out->size);
    if(!hm_get(&ctx->cache->textures, out->hash)){
        RB_CpuGpuSpan staging = rn_upload_reserve(ctx->cache->renderer, cast(i64)out->size);
        MEMORY_COPY(staging.cpu, blocks, out->size);
        dot_asset_cache_add_texture(ctx->cache, out, staging);
    }
}

internal void
dot_model_bake_texture_flush(DOT_ModelBakeCtx *ctx)
{
    if(!ctx->pending_count){
        return;
    }
    RN_RenderCtx *renderer = ctx->cache->renderer;
    DOT_PROFILE_BLOCK("Bake texture flush"){
        rn_upload_flush(renderer);
    }
    for(u32 i = 0; i < ctx->pending_count; ++i){
        dot_model_bake_texture_finish(ctx, ctx->pending[i].out, ctx->pending[i].blocks);
    }
    ctx->pending_count = 0;
    rn_scratch_reset(renderer);
    rn_readback_reset(renderer);
}

internal void
dot_model_bake_texture_make_room(DOT_ModelBakeCtx *ctx, i64 scratch_size, i64 readback_size)
{
    RN_RenderCtx *renderer = ctx->cache->renderer;
    if(!rn_scratch_fits(renderer, scratch_size) || !rn_readback_fits(renderer, readback_size)){
        dot_model_bake_texture_flush(ctx);
    }
}

internal b32
dot_model_bake_texture(DOT_ModelBakeCtx *ctx, String8 path, DOT_ModelBakeTextureUse use, DOT_ModelFileTexture *out)
{
    RN_RenderCtx             *renderer  = ctx->cache->renderer;
    DOT_TextureBakePipelines *pipelines = &ctx->textures;
    b32 srgb = DOT_BITS_ANY(use, DOT_ModelBakeTextureUse_Color);
    TempArena temp = temp_arena_start(ctx->arena);
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
    b32 use_bc = pipelines->ready && !is_16_bit;
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
    if(use_bc){
        b32 bc5 = use == DOT_ModelBakeTextureUse_Normal;
        DOT_TextureBakeLayout layout = {0};
        dot_texture_bake_layout_compute(&layout, width, height, mip_count, out);
        i64 scratch_size = layout.pixels_size + (bc5 ? 0 : dot_texture_bake_bc7_candidates_size(&layout));
        dot_model_bake_texture_make_room(ctx, scratch_size, cast(i64)out->size);

        u8 *blocks = NULL;
        if(bc5){
            DOT_PROFILE_BLOCK("Bake BC5 record"){
                blocks = dot_texture_bake_bc5(renderer, pipelines, &layout, cast(u8*)data, out);
            }
            if(dot_g_bake_report_bc5_quality){
                rn_upload_flush(renderer);
                f64 psnr = dot_bc5_psnr(blocks, cast(u8*)data, cast(u32)width, cast(u32)height);
                DOT_PRINT("BC5 %S: %.2f dB PSNR on mip 0", path, psnr);
            }
        }else{
            DOT_PROFILE_BLOCK("Bake BC7 record"){
                blocks = dot_texture_bake_bc7(renderer, pipelines, &layout, cast(u8*)data, srgb, out);
            }
        }
        ctx->pending[ctx->pending_count++] = (DOT_TextureBakePending){ .out = out, .blocks = blocks };
        temp_arena_restore(temp);
        return(true);
    }

    u8 *block = NULL;
    {
        u8 *chain = cast(u8*)data;
        if(mip_count > 1){
            DOT_PROFILE_BLOCK("Bake mip chain"){
                chain = rn_mip_chain_build(temp.arena, data, width, height, comp, size_bytes, mip_count); 
            }
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

    dot_model_bake_texture_finish(ctx, out, block);
    temp_arena_restore(temp);
    return(true);
}

////////////////////////////////////////////////////////////////
/// DOT_MODEL_BAKE_MATERIAL_FUNCTIONS

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

////////////////////////////////////////////////////////////////
/// DOT_MODEL_BAKE_FUNCTIONS

internal DOT_Model
dot_model_bake(DOT_AssetCache *cache, String8 gltf_path, String8 baked_path, b32 compress)
{
    DOT_Model model = {0};
    Scratch s = scratch_begin(0,0);
    cgltf_data *gltf_data = NULL;
    DOT_PROFILE_BLOCK("Bake glTF load"){
        gltf_data = dot_gltf_load_from_path(s.arena, gltf_path);
    }
    if(!gltf_data || !gltf_data->scene){
        DOT_WARNING("Could not bake %S, failed to load the glTF or it has no default scene", gltf_path);
        scratch_end(s);
        return(model);
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
    DOT_ModelBakeCtx ctx = {0};
    ctx.arena       = s.arena;
    ctx.cache       = cache;
    ctx.file        = platform_file_open_write(tmp_path);
    ctx.file_ok     = ctx.file.handle != NULL;
    ctx.file_cursor = cursor;
    ctx.blocks      = dot_block_table_create(s.arena, cast(u64)header.texture_count + header.mesh_count);
    ctx.pending     = PUSH_ARRAY(s.arena, DOT_TextureBakePending, header.texture_count);

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

    if(compress && header.texture_count){
        dot_texture_bake_pipelines_init(cache->renderer, &ctx.textures);
    }
    rn_upload_begin(cache->renderer);
    String8 folder = string8_chop_last_slash(gltf_path);
    DOT_PROFILE_BLOCK("Bake textures"){
        for(u32 i = 0; i < header.texture_count; ++i){
            String8 image_path = string8_format(s.arena, "%S/%s", folder, gltf_data->images[i].uri);
            texture_valid[i] = dot_model_bake_texture(&ctx, image_path, texture_use[i], &textures[i]);
        }
        dot_model_bake_texture_flush(&ctx);
    }
    if(compress && header.texture_count){
        dot_texture_bake_pipelines_end(cache->renderer, &ctx.textures);
    }
    DOT_PROFILE_BLOCK("Bake meshes"){
        for(cgltf_size m = 0; m < gltf_data->meshes_count; ++m){
            cgltf_mesh *mesh = &gltf_data->meshes[m];
            for(cgltf_size p = 0; p < mesh->primitives_count; ++p){
                u32 mesh_index = mesh_first[m] + cast(u32)p;
                mesh_valid[mesh_index] = dot_model_bake_mesh(&ctx, &mesh->primitives[p], &meshes[mesh_index]);
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
    header.file_size     = cast(u64)ctx.file.size;

    DOT_ModelFileTables tables = {
        .header    = header,
        .meshes    = meshes,
        .submeshes = submeshes,
        .materials = materials,
        .textures  = textures,
        .samplers  = samplers,
    };
    model = dot_model_from_tables(cache, &tables);
    DOT_PROFILE_BLOCK("Bake upload submit"){
        rn_upload_end(cache->renderer);
    }

    if(ctx.file_ok){
        ctx.file_ok = platform_file_write_at(&ctx.file, cast(i64)header.meshes_offset,    meshes,    cast(i64)(header.mesh_count     * sizeof(DOT_ModelFileMesh)))
                   && platform_file_write_at(&ctx.file, cast(i64)header.submeshes_offset, submeshes, cast(i64)(header.submesh_count  * sizeof(DOT_ModelFileSubmesh)))
                   && platform_file_write_at(&ctx.file, cast(i64)header.materials_offset, materials, cast(i64)(header.material_count * sizeof(DOT_ModelFileMaterial)))
                   && platform_file_write_at(&ctx.file, cast(i64)header.textures_offset,  textures,  cast(i64)(header.texture_count  * sizeof(DOT_ModelFileTexture)))
                   && platform_file_write_at(&ctx.file, cast(i64)header.samplers_offset,  samplers,  cast(i64)(header.sampler_count  * sizeof(DOT_ModelFileSampler)))
                   && platform_file_write_at(&ctx.file, 0, &header, sizeof(header));
    }
    platform_file_close(&ctx.file);

    if(ctx.file_ok){
        remove(baked_path.cstr);
        ctx.file_ok = rename(tmp_path.cstr, baked_path.cstr) == 0;
    }
    if(!ctx.file_ok){
        DOT_WARNING("Could not write baked model %S, the model is loaded but will be baked again next run", baked_path);
        remove(tmp_path.cstr);
    }else{
        DOT_PRINT("Baked %S: %M, %M of duplicate blocks shared instead of written", baked_path, cast(i64)header.file_size, cast(i64)ctx.blocks.deduped_bytes);
    }
    scratch_end(s);
    return(model);
}

////////////////////////////////////////////////////////////////
/// DOT_ASSET_CACHE_FUNCTIONS

internal void
dot_asset_cache_init(Arena *arena, RN_RenderCtx *renderer, DOT_AssetCache *cache)
{
    DOT_ASSERT(!cache->initialized, "Asset cache initialized twice");
    cache->arena    = arena;
    cache->renderer = renderer;
    hm_init(arena, &cache->textures, 256, hm_hasheq_bytes);
    hm_init(arena, &cache->meshes,   256, hm_hasheq_bytes);
    os_directory_create_if_not_exists(str_comp("build"));
    os_directory_create_if_not_exists(str_comp(DOT_MODEL_CACHE_PATH));
    cache->initialized = true;
}

internal void
dot_asset_cache_end(DOT_AssetCache *cache)
{
    if(!cache->initialized){
        return;
    }
    HashMapIter it = {0};
    for(RN_BindlessTexture *texture = hm_iter_next(&cache->textures, &it); texture; texture = hm_iter_next(&cache->textures, &it)){
        rn_texture_destroy(cache->renderer, texture->handle);
    }
    MEMORY_ZERO_STRUCT(cache);
}

internal void
dot_asset_cache_add_texture(DOT_AssetCache *cache, DOT_ModelFileTexture *entry, RB_CpuGpuSpan staging)
{
    Scratch s = scratch_begin(0,0);
    RB_TextureDesc desc = {0};
    desc.dimension_kind = RB_TextureDimensionKind_2D;
    desc.format_kind    = cast(RB_FormatKind)entry->format_kind;
    desc.mip_levels     = cast(u8)entry->mip_count;
    desc.width          = cast(u16)entry->width;
    desc.height         = cast(u16)entry->height;
    desc.depth          = 1;
    desc.array_layers   = 1;
    desc.debug_name     = string8_format(s.arena, "tex %08" PRIx64, entry->hash.lo & 0xffffffffull);
    RN_BindlessTexture texture = rn_texture_create_bindless_in_upload(cache->renderer, &desc);
    for(u32 mip = 0; mip < entry->mip_count; ++mip){
        RB_GpuRange mip_range = { staging.gpu.gpu + entry->mip_offsets[mip], cast(i64)entry->mip_sizes[mip] };
        rn_upload_copy_texture(cache->renderer, mip_range, texture.handle, (RB_TextureCopyDesc){ .mip_level = mip });
    }
    *hm_get_or_create(cache->arena, &cache->textures, entry->hash) = texture;
    scratch_end(s);
}

internal void
dot_asset_cache_add_mesh(DOT_AssetCache *cache, DOT_ModelFileMesh *entry, RB_CpuGpuSpan staging)
{
    RB_GpuAddress address = rn_data_alloc(cache->renderer, cast(i64)entry->size);
    rn_upload_copy_memory(cache->renderer, staging.gpu, address);
    *hm_get_or_create(cache->arena, &cache->meshes, entry->hash) = address;
}

////////////////////////////////////////////////////////////////
/// DOT_MODEL_LOAD_FUNCTIONS

internal b32
dot_model_file_read_table(PlatformFile *file, Arena *arena, u64 offset, u64 size, void **out)
{
    *out = PUSH_SIZE(arena, size ? size : 1);
    return(size == 0 || platform_file_read_at(file, cast(i64)offset, *out, cast(i64)size));
}

internal DOT_Model
dot_model_load_baked(DOT_AssetCache *cache, String8 baked_path)
{
    DOT_Model model = {0};
    DOT_ASSERT(cache->initialized, "dot_asset_cache_init must run before loading models");

    PlatformFile file = platform_file_open_read(baked_path);
    if(!file.handle){
        return(model);
    }
    DOT_ModelFileTables tables = {0};
    DOT_ModelFileHeader *header = &tables.header;
    if(!platform_file_read_at(&file, 0, header, sizeof(*header)) || header->magic != DOT_MODEL_FILE_MAGIC ||
       header->version != DOT_MODEL_FILE_VERSION || header->file_size != cast(u64)file.size){
        DOT_WARNING("Baked model %S is invalid or out of date", baked_path);
        platform_file_close(&file);
        return(model);
    }

    Scratch s = scratch_begin(0,0);
    b32 tables_ok = dot_model_file_read_table(&file, s.arena, header->meshes_offset,    header->mesh_count     * sizeof(DOT_ModelFileMesh),     cast(void**)&tables.meshes)
                 && dot_model_file_read_table(&file, s.arena, header->submeshes_offset, header->submesh_count  * sizeof(DOT_ModelFileSubmesh),  cast(void**)&tables.submeshes)
                 && dot_model_file_read_table(&file, s.arena, header->materials_offset, header->material_count * sizeof(DOT_ModelFileMaterial), cast(void**)&tables.materials)
                 && dot_model_file_read_table(&file, s.arena, header->textures_offset,  header->texture_count  * sizeof(DOT_ModelFileTexture),  cast(void**)&tables.textures)
                 && dot_model_file_read_table(&file, s.arena, header->samplers_offset,  header->sampler_count  * sizeof(DOT_ModelFileSampler),  cast(void**)&tables.samplers);
    if(!tables_ok){
        DOT_WARNING("Could not read the tables of %S", baked_path);
        platform_file_close(&file);
        scratch_end(s);
        return(model);
    }

    u32 textures_uploaded = 0;
    u32 meshes_uploaded   = 0;
    rn_upload_begin(cache->renderer);

    DOT_PROFILE_BLOCK("Baked textures"){
        for(u32 i = 0; i < header->texture_count; ++i){
            DOT_ModelFileTexture *entry = &tables.textures[i];
            if(!entry->mip_count || hm_get(&cache->textures, entry->hash)){
                continue;
            }
            RB_CpuGpuSpan staging = rn_upload_reserve(cache->renderer, cast(i64)entry->size);
            if(!platform_file_read_at(&file, cast(i64)entry->offset, staging.cpu, cast(i64)entry->size)){
                DOT_WARNING("Could not read texture %u of %S", i, baked_path);
            }
            dot_asset_cache_add_texture(cache, entry, staging);
            textures_uploaded += 1;
        }
    }

    DOT_PROFILE_BLOCK("Baked meshes"){
        for(u32 i = 0; i < header->mesh_count; ++i){
            DOT_ModelFileMesh *entry = &tables.meshes[i];
            if(!entry->size || hm_get(&cache->meshes, entry->hash)){
                continue;
            }
            RB_CpuGpuSpan staging = rn_upload_reserve(cache->renderer, cast(i64)entry->size);
            if(!platform_file_read_at(&file, cast(i64)entry->offset, staging.cpu, cast(i64)entry->size)){
                DOT_WARNING("Could not read mesh %u of %S", i, baked_path);
            }
            dot_asset_cache_add_mesh(cache, entry, staging);
            meshes_uploaded += 1;
        }
    }

    model = dot_model_from_tables(cache, &tables);
    DOT_PROFILE_BLOCK("Baked upload submit"){
        rn_upload_end(cache->renderer);
    }

    DOT_PRINT("Loaded %S: %u/%u textures and %u/%u meshes uploaded, the rest reused from the cache",
        baked_path, textures_uploaded, header->texture_count, meshes_uploaded, header->mesh_count);
    platform_file_close(&file);
    scratch_end(s);
    return(model);
}

internal DOT_Model
dot_model_from_tables(DOT_AssetCache *cache, DOT_ModelFileTables *tables)
{
    RN_RenderCtx        *renderer = cache->renderer;
    DOT_ModelFileHeader *header   = &tables->header;
    DOT_Model model = {0};
    Scratch s = scratch_begin(0,0);
    u32           *texture_slots  = PUSH_ARRAY(s.arena, u32, header->texture_count);
    u32           *sampler_slots  = PUSH_ARRAY(s.arena, u32, header->sampler_count);
    RB_GpuAddress *mesh_addresses = PUSH_ARRAY(s.arena, RB_GpuAddress, header->mesh_count);

    for(u32 i = 0; i < header->texture_count; ++i){
        DOT_ModelFileTexture *entry = &tables->textures[i];
        if(!entry->mip_count){
            continue;
        }
        RN_BindlessTexture *cached = hm_get(&cache->textures, entry->hash);
        DOT_ASSERT(cached, "Texture %u must be in the asset cache before building the model", i);
        texture_slots[i] = cached->bindless_slot;
    }
    for(u32 i = 0; i < header->mesh_count; ++i){
        DOT_ModelFileMesh *entry = &tables->meshes[i];
        if(!entry->size){
            continue;
        }
        RB_GpuAddress *cached = hm_get(&cache->meshes, entry->hash);
        DOT_ASSERT(cached, "Mesh %u must be in the asset cache before building the model", i);
        mesh_addresses[i] = *cached;
    }

    for(u32 i = 0; i < header->sampler_count; ++i){
        DOT_ModelFileSampler *entry = &tables->samplers[i];
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

    DOT_MaterialGpu *material_gpus = PUSH_ARRAY(s.arena, DOT_MaterialGpu, header->material_count);
    for(u32 i = 0; i < header->material_count; ++i){
        DOT_ModelFileMaterial *entry = &tables->materials[i];
        material_gpus[i] = entry->gpu;
        for EACH_INDEX(k, DOT_PBRTextureKind_Count){
            u32 texture_index = entry->texture_indices[k];
            u32 sampler_index = entry->sampler_indices[k];
            material_gpus[i].tex_slots[k]     = texture_index != DOT_MODEL_FILE_NONE ? texture_slots[texture_index] : 0;
            material_gpus[i].sampler_slots[k] = sampler_index != DOT_MODEL_FILE_NONE ? sampler_slots[sampler_index] : 0;
        }
    }
    i64 materials_size = cast(i64)(header->material_count * sizeof(DOT_MaterialGpu));
    RB_GpuAddress materials_address = rn_data_alloc(renderer, materials_size);
    rn_upload_memory(renderer, materials_address, material_gpus, materials_size);

    model.sub_mesh_draw_count = header->submesh_count;
    model.sub_mesh_draw = PUSH_ARRAY(cache->arena, DOT_Submesh, header->submesh_count);
    for(u32 i = 0; i < header->submesh_count; ++i){
        DOT_ModelFileSubmesh *entry = &tables->submeshes[i];
        DOT_ModelFileMesh *mesh = &tables->meshes[entry->mesh_index];
        RB_GpuAddress base = mesh_addresses[entry->mesh_index];
        DOT_Submesh *sub_mesh = &model.sub_mesh_draw[i];
        sub_mesh->transform     = entry->transform;
        sub_mesh->index_count   = mesh->index_count;
        sub_mesh->index_addr    = base + mesh->index_offset;
        sub_mesh->position_addr = base + mesh->position_offset;
        sub_mesh->normal_addr   = base + mesh->normal_offset;
        sub_mesh->texcoord_addr = base + mesh->texcoord_offset;
        sub_mesh->tangent_addr  = mesh->tangent_offset ? base + mesh->tangent_offset : 0;
        sub_mesh->material_addr = materials_address + entry->material_index * sizeof(DOT_MaterialGpu);
    }
    scratch_end(s);
    return(model);
}

internal DOT_Model
dot_model_load(DOT_AssetCache *cache, String8 gltf_path)
{
    Scratch s = scratch_begin(0,0);
    DOT_ASSERT(cache->initialized, "dot_asset_cache_init must run before loading models");
    String8 baked_path = dot_model_baked_path(s.arena, gltf_path);

    b32 baked = !platform_file_is_newer(gltf_path, baked_path);
    if(baked){
        PlatformFile file = platform_file_open_read(baked_path);
        DOT_ModelFileHeader header = {0};
        baked = file.handle && platform_file_read_at(&file, 0, &header, sizeof(header)) &&
                header.magic == DOT_MODEL_FILE_MAGIC && header.version == DOT_MODEL_FILE_VERSION;
        platform_file_close(&file);
    }
    DOT_Model model = {0};
    if(baked){
        DOT_PROFILE_BLOCK("Baked model load"){
            model = dot_model_load_baked(cache, baked_path);
        }
    }else{
        DOT_PRINT("Baking %S into %S", gltf_path, baked_path);
        DOT_PROFILE_BLOCK("Model bake"){
            model = dot_model_bake(cache, gltf_path, baked_path, rb_device_info().texture_compression_bc);
        }
    }
    scratch_end(s);
    return(model);
}

internal void
dot_model_destroy(DOT_AssetCache *cache, DOT_Model *model)
{
    for EACH_INDEX(i, model->texture_count){
        rn_texture_destroy(cache->renderer, model->textures[i]);
    }
}
