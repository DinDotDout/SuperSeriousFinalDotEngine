internal DOT_Scene
dot_scene_from_gltf(RN_RenderCtx *renderer, const cgltf_data *gltf_data, String8 gltf_path)
{
    DOT_ASSERT(gltf_data->scene, "No default scene");
    DOT_ASSERT(gltf_data->textures_count <= U32_MAX); DOT_ASSERT(gltf_data->meshes_count <= U32_MAX);
    DOT_ASSERT(gltf_data->samplers_count <= U32_MAX); DOT_ASSERT(gltf_data->buffers_count <= U32_MAX);
    DOT_ASSERT(gltf_data->materials_count <= U32_MAX);

    TempArena temp = threadctx_temp_begin(0,0);

    DOT_Scene scene = {0};
    scene.texture_count = cast(u32)gltf_data->textures_count;
    scene.buffer_count  = cast(u32)gltf_data->buffers_count;
    scene.sampler_count = cast(u32)gltf_data->samplers_count;
    // scene.mesh_count    = cast(u32)gltf_data->meshes_count;
    // scene.material_count = cast(u32)gltf_data->materials_count;

    scene.textures  = PUSH_ARRAY(renderer->permanent_arena, RN_Texture, scene.texture_count);
    scene.buffers   = PUSH_ARRAY(renderer->permanent_arena, RN_Buffer, scene.buffer_count);
    scene.samplers  = PUSH_ARRAY(renderer->permanent_arena, RN_Sampler, scene.sampler_count);
    // scene.meshes    = PUSH_ARRAY(renderer->transient_arena, DOT_Mesh, scene.mesh_count);
    // scene.materials = PUSH_ARRAY(renderer->transient_arena, DOT_MaterialPBR, scene.material_count);
    scene.sub_mesh_draw = PUSH_ARRAY(renderer->permanent_arena, DOT_Submesh, gltf_data->nodes_count);

    String8 folder = string8_chop_last_slash(gltf_path);

    // (jd) NOTE: For now we are going to assume that all images are on a separate path
    for(u64 i = 0; i < gltf_data->images_count; ++i){
        cgltf_image *image = &gltf_data->images[i];
        String8 name = image->name ? string8_from_cstring(image->name) : string8_lit("Default");
        String8 full_path = string8_format(temp.arena, "%S/%s", folder, image->uri);
        DOT_PRINT("name: %S, image uri: %S", name, full_path);

        scene.textures[i] = rn_texture_load_from_path(renderer, name, full_path, 0);
    }

    for(u64 i = 0; i < gltf_data->samplers_count; ++i){
        cgltf_sampler *sampler = &gltf_data->samplers[i];
        String8 name = sampler->name ? string8_from_cstring(sampler->name) : string8_lit("Default");

        RN_SamplerDesc desc = {0};
        desc.min_filter = sampler->min_filter == cgltf_filter_type_linear ? RN_SamplerFilterKind_Linear : RN_SamplerFilterKind_Nearest;

        switch(sampler->min_filter){
        case cgltf_filter_type_undefined: break;
        case cgltf_filter_type_nearest:
            desc.min_filter = RN_SamplerFilterKind_Nearest;
        break;
        case cgltf_filter_type_linear:
            desc.min_filter = RN_SamplerFilterKind_Linear;
        break;
        case cgltf_filter_type_linear_mipmap_nearest:
            desc.min_filter = RN_SamplerFilterKind_Linear;
            desc.mipmap_filter = RN_SamplerMipmapFilterKind_Nearest;
        break;
        case cgltf_filter_type_linear_mipmap_linear:
            desc.min_filter = RN_SamplerFilterKind_Linear;
            desc.mipmap_filter = RN_SamplerMipmapFilterKind_Linear;
        break;
        case cgltf_filter_type_nearest_mipmap_nearest:
            desc.min_filter = RN_SamplerFilterKind_Nearest;
            desc.mipmap_filter = RN_SamplerMipmapFilterKind_Nearest;
        break;
        case cgltf_filter_type_nearest_mipmap_linear:
            desc.min_filter = RN_SamplerFilterKind_Nearest;
            desc.mipmap_filter = RN_SamplerMipmapFilterKind_Linear;
        break;
        }
        desc.mag_filter = sampler->mag_filter == cgltf_filter_type_linear ? RN_SamplerFilterKind_Linear : RN_SamplerFilterKind_Nearest;
        switch (sampler->wrap_s){
        case cgltf_wrap_mode_clamp_to_edge  : desc.address_mode_u = RN_SamplerAddressModeKind_ClampToEdge; break;
        case cgltf_wrap_mode_mirrored_repeat: desc.address_mode_u = RN_SamplerAddressModeKind_MirroredRepeat; break;
        case cgltf_wrap_mode_repeat         : desc.address_mode_u = RN_SamplerAddressModeKind_Repeat; break;
        }

        switch (sampler->wrap_t){
        case cgltf_wrap_mode_clamp_to_edge  : desc.address_mode_v = RN_SamplerAddressModeKind_ClampToEdge; break;
        case cgltf_wrap_mode_mirrored_repeat: desc.address_mode_v = RN_SamplerAddressModeKind_MirroredRepeat; break;
        case cgltf_wrap_mode_repeat         : desc.address_mode_v = RN_SamplerAddressModeKind_Repeat; break;
        }
        DOT_DebugNameSet(desc.debug_name, name);

        scene.samplers[i] = rn_sampler_create(renderer, &desc);
    }

    // (jd) NOTE: We already loaded buffers with cgltf_load_buffers, maybe do this ourselves?
    for(u64 i = 0; i < gltf_data->buffer_views_count; ++i){
        cgltf_buffer_view *buffer_view = &gltf_data->buffer_views[i];
        cgltf_buffer *buffer = buffer_view->buffer;
        u8 *buffer_data = cast(u8*)buffer->data + buffer_view->offset;

        RN_BufferDesc desc = {0};
        desc.size               = buffer_view->size;
        desc.resource_usage     = RN_ResourceUsageKind_GPUOnly;
        desc.buffer_usage_flags = RN_BufferUsageBit_Vertex | RN_BufferUsageBit_Index;

        String8 name = buffer_view->name ? string8_from_cstring(buffer_view->name) : string8_lit("Default");
        DOT_DebugNameSet(desc.debug_name, name);

        scene.buffers[i] = rn_buffer_create(renderer, &desc, buffer_data);
    }

	cgltf_scene *root_scene = gltf_data->scene; // (jd) using only one for now cgltf_node** nodes;

    for(cgltf_size ri = 0; ri < root_scene->nodes_count; ++ri){
        cgltf_node *gltf_node = root_scene->nodes[ri];
        if(gltf_node->mesh == NULL){
            DOT_TODO("Handle camera / light nodes");
            continue;
        }

        cgltf_mesh *gltf_mesh = gltf_node->mesh;

        vec3 node_scale = vec3_one;
        if(gltf_node->has_scale){
            node_scale = v3(gltf_node->scale[0], gltf_node->scale[1], gltf_node->scale[2]);
        }

        for(u32 primitive_index = 0; primitive_index < gltf_mesh->primitives_count; ++primitive_index){
            cgltf_primitive *primitive = &gltf_mesh->primitives[primitive_index];
	        const cgltf_accessor *index_accessor        = primitive->indices;
            const cgltf_accessor *position_accessor     = cgltf_find_accessor(primitive, cgltf_attribute_type_position, 0);
            const cgltf_accessor *tangent_accessor      = cgltf_find_accessor(primitive, cgltf_attribute_type_tangent, 0);
            const cgltf_accessor *normal_accessor       = cgltf_find_accessor(primitive, cgltf_attribute_type_normal, 0);
            const cgltf_accessor *texcoord0_accessor    = cgltf_find_accessor(primitive, cgltf_attribute_type_texcoord, 0);

            DOT_Submesh *sub_mesh = &scene.sub_mesh_draw[scene.sub_mesh_draw_count];


            RN_ShaderResourceDesc *shader_resource = &sub_mesh->shader_resource;
            rn_shader_resource_desc_set_layout(shader_resource, g_shader_resource_layout_h);
            rn_shader_resource_desc_bind_buffer(shader_resource, g_cube_ubo_h, 0);

            sub_mesh->scale = node_scale;

            DOT_ASSERT(index_accessor->count < U32_MAX);
            sub_mesh->index_count = cast(u32)index_accessor->count;

            { // Bind material buffer
                RN_BufferDesc desc = {0};
                desc.buffer_usage_flags = RN_BufferUsageBit_Uniform;
                desc.resource_usage = RN_ResourceUsageKind_Dynamic;
                desc.size = sizeof(DOT_MaterialPBR);
                DOT_DebugNameSet(desc.debug_name, string8_lit("material"));
                sub_mesh->buffers[DOT_PrimitiveBufferKind_Material] = rn_buffer_create(renderer, &desc, 0).handle;
            }

            if(index_accessor){
               sub_mesh->buffers[DOT_PrimitiveBufferKind_Index] = scene.buffers[cgltf_buffer_index(gltf_data, index_accessor->buffer_view->buffer)].handle;
               sub_mesh->buffer_offsets[DOT_PrimitiveBufferKind_Index] = cast(u32)(index_accessor->offset + index_accessor->buffer_view->offset);
            }
            if(position_accessor){
                sub_mesh->buffers[DOT_PrimitiveBufferKind_Position] = scene.buffers[cgltf_buffer_index(gltf_data, position_accessor->buffer_view->buffer)].handle;
                sub_mesh->buffer_offsets[DOT_PrimitiveBufferKind_Position] = cast(u32)(position_accessor->offset + position_accessor->buffer_view->offset);
            }
            if(normal_accessor){
                sub_mesh->buffers[DOT_PrimitiveBufferKind_Normal] = scene.buffers[cgltf_buffer_index(gltf_data, normal_accessor->buffer_view->buffer)].handle;
                sub_mesh->buffer_offsets[DOT_PrimitiveBufferKind_Normal] = cast(u32)(normal_accessor->offset + normal_accessor->buffer_view->offset);
            }else{
                DOT_TODO("Provide fallback");
            }

            if(tangent_accessor){
                sub_mesh->buffers[DOT_PrimitiveBufferKind_Tangent] = scene.buffers[cgltf_buffer_index(gltf_data, tangent_accessor->buffer_view->buffer)].handle;
                sub_mesh->buffer_offsets[DOT_PrimitiveBufferKind_Tangent] = cast(u32)(tangent_accessor->offset + tangent_accessor->buffer_view->offset);
                sub_mesh->material_data.flags |= DOT_MaterialFeatureBit_TangentVertexAttribute;
            }else{
                DOT_TODO("Provide fallback");
            }

            if(texcoord0_accessor){
                sub_mesh->buffer_offsets[DOT_PrimitiveBufferKind_Texcoord] = cast(u32)(texcoord0_accessor->offset + texcoord0_accessor->buffer_view->offset);
                sub_mesh->buffers[DOT_PrimitiveBufferKind_Texcoord] = scene.buffers[cgltf_buffer_index(gltf_data, texcoord0_accessor->buffer_view->buffer)].handle;
                sub_mesh->material_data.flags |= DOT_MaterialFeatureBit_TexcoordVertexAttribute;
            }else{
                DOT_TODO("Provide fallback");
            }

            cgltf_material *gltf_material = primitive->material;
            DOT_ASSERT(gltf_material);
            sub_mesh->material_data.base_color_factor  = v4(1,1,1,1);
            sub_mesh->material_data.metallic_factor    = 1.0f;
            sub_mesh->material_data.roughness_factor   = 1.0f;
            sub_mesh->material_data.emissive_factor    = v3(0,0,0);
            sub_mesh->material_data.occlusion_factor   = 1.0f;

            RN_TextureHandle dummy_texture = {0};
            RN_SamplerHandle dummy_sampler = {0};
            if(gltf_material->has_pbr_metallic_roughness){
                cgltf_pbr_metallic_roughness *pbr = &gltf_material->pbr_metallic_roughness;

                sub_mesh->material_data.base_color_factor = v4(pbr->base_color_factor[0],
                    pbr->base_color_factor[1],
                    pbr->base_color_factor[2],
                    pbr->base_color_factor[3]);
                sub_mesh->material_data.metallic_factor = pbr->metallic_factor;
                sub_mesh->material_data.roughness_factor = pbr->roughness_factor;

                if(pbr->base_color_texture.texture){
                    DOT_ASSERT(pbr->base_color_texture.texture->image);
                    rn_shader_resource_desc_bind_texture_sampler(shader_resource,
                        scene.textures[cgltf_image_index(gltf_data, pbr->base_color_texture.texture->image)].handle,
                        scene.samplers[cgltf_sampler_index(gltf_data, pbr->base_color_texture.texture->sampler)].handle, 
                        2);
                    sub_mesh->material_data.flags |= DOT_MaterialFeatureBit_ColorTexture;
                }else{
                    DOT_TODO("Provide fallback");
                    rn_shader_resource_desc_bind_texture_sampler(shader_resource, dummy_texture, dummy_sampler, 2);
                }

                if(pbr->metallic_roughness_texture.texture){
                    DOT_ASSERT(pbr->base_color_texture.texture->image);
                    rn_shader_resource_desc_bind_texture_sampler(shader_resource,
                        scene.textures[cgltf_image_index(gltf_data, pbr->metallic_roughness_texture.texture->image)].handle,
                        scene.samplers[cgltf_sampler_index(gltf_data, pbr->metallic_roughness_texture.texture->sampler)].handle,
                        3);
                    sub_mesh->material_data.flags |= DOT_MaterialFeatureBit_RoughnessTexture;
                }else{
                    DOT_TODO("Provide fallback");
                    rn_shader_resource_desc_bind_texture_sampler(shader_resource, dummy_texture, dummy_sampler, 3);
                }
                sub_mesh->material_data.metallic_factor    = pbr->metallic_factor;
                sub_mesh->material_data.roughness_factor   = pbr->roughness_factor;
            }

            cgltf_texture_view *occlusion_view = &gltf_material->occlusion_texture;
            if(occlusion_view->texture && occlusion_view->texture->image){
                rn_shader_resource_desc_bind_texture_sampler(shader_resource,
                    scene.textures[cgltf_image_index(gltf_data, occlusion_view->texture->image)].handle,
                    scene.samplers[cgltf_sampler_index(gltf_data, occlusion_view->texture->sampler)].handle,
                    4);
                sub_mesh->material_data.occlusion_factor = occlusion_view->scale; // always valid (default = 1.0f)
                sub_mesh->material_data.flags |= DOT_MaterialFeatureBit_OcclusionTexture;
            }else{
                rn_shader_resource_desc_bind_texture_sampler(shader_resource, dummy_texture, dummy_sampler, 4);
                // DOT_TODO("Provide fallback");
            }
            cgltf_texture_view *emissive_view = &gltf_material->emissive_texture;
            if(emissive_view->texture && occlusion_view->texture->image){
                rn_shader_resource_desc_bind_texture_sampler(shader_resource,
                    scene.textures[cgltf_image_index(gltf_data, emissive_view->texture->image)].handle,
                    scene.samplers[cgltf_sampler_index(gltf_data, emissive_view->texture->sampler)].handle,
                    5);
                sub_mesh->material_data.emissive_factor = v3(gltf_material->emissive_factor[0], gltf_material->emissive_factor[1], gltf_material->emissive_factor[2]); // always valid (default = 1.0f)
                sub_mesh->material_data.flags |= DOT_MaterialFeatureBit_EmissiveTexture;
            }else{
                rn_shader_resource_desc_bind_texture_sampler(shader_resource, dummy_texture, dummy_sampler, 5);
                // DOT_TODO("Provide fallback");
            }
            cgltf_texture_view *normal_view = &gltf_material->normal_texture;
            if(normal_view->texture && normal_view->texture->image){
                rn_shader_resource_desc_bind_texture_sampler(shader_resource,
                    scene.textures[cgltf_image_index(gltf_data, normal_view->texture->image)].handle,
                    scene.samplers[cgltf_sampler_index(gltf_data, normal_view->texture->sampler)].handle,
                    6);
                sub_mesh->material_data.flags |= DOT_MaterialFeatureBit_NormalTexture;
            }else{
                rn_shader_resource_desc_bind_texture_sampler(shader_resource, dummy_texture, dummy_sampler, 6);
                // DOT_TODO("Provide fallback");
            }
            rn_shader_resource_create(renderer, shader_resource);
            scene.sub_mesh_draw_count += 1;
        }
    }

    threadctx_temp_end(temp);
    return(scene);
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
    cgltf_result result = cgltf_parse(&options, buff.str, buff.size, &gltf_data);
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

internal DOT_Scene
dot_scene_load_from_path(RN_RenderCtx *renderer, String8 path)
{
    TempArena temp = threadctx_temp_begin(0,0);
    cgltf_data *gltf = dot_gltf_load_from_path(temp.arena, path);
    DOT_Scene scene = {0};
    if(gltf){
        scene = dot_scene_from_gltf(renderer, gltf, path);
        // cgltf_free(gltf);
    }
    threadctx_temp_end(temp);
	return(scene);
}
