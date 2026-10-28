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

        RN_BindlessTexture loaded = rn_texture_load_from_path(renderer, name, full_path, 0);
        model.textures[i] = loaded.handle;
        texture_slots[i]  = loaded.bindless_slot;
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

        sampler_slots[i] = rn_sampler_bindless_slot_create(renderer, &desc);
    }

    cgltf_scene *root_scene = gltf_data->scene;

    rn_upload_begin(renderer);

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

    rn_upload_end(renderer);

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
    cgltf_result result = cgltf_parse(&options, buff.str, cast(u64)buff.size, &gltf_data);
    b32 fail_parse = false;
    if(result != cgltf_result_success){
        fail_parse = true;
        DOT_WARNING("cgltf_parse failed: %d", result);
    }
    b32 load_buffers_failed = false;
    if(!fail_parse){
        result = cgltf_load_buffers(&options, gltf_data, path.cstr);
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
