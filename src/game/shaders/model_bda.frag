#include "renderer/shaders/shader_shared.h"
#include "game/shaders/model_shared.h"

struct VSOutput {
    float4 sv_position : SV_Position;
    float3 normal      : NORMAL0;
    float2 texcoord    : TEXCOORD0;
    float3 world_pos   : TEXCOORD1;
};

float sun_shadow(DOT_ShadowGpu *shadow, float3 world_pos, float3 normal)
{
    if(shadow->enabled == 0){
        return 1.0;
    }
    float4 light_clip = mul(shadow->light_view_proj, float4(world_pos + normal * shadow->normal_offset, 1.0));
    float3 light_ndc  = light_clip.xyz / light_clip.w;
    float2 uv         = light_ndc.xy * 0.5 + 0.5;
    if(any(uv < 0.0) || any(uv > 1.0) || light_ndc.z > 1.0){
        return 1.0;
    }

    Texture2D shadow_map = ResourceDescriptorHeap[shadow->shadow_slot];
    int2  center   = int2(uv * float(shadow->map_size));
    int   max_texel = int(shadow->map_size) - 1;
    float receiver = light_ndc.z - shadow->depth_bias;
    float lit      = 0.0;
    for(int y = -1; y <= 1; y++){
        for(int x = -1; x <= 1; x++){
            int2  texel   = clamp(center + int2(x, y), int2(0, 0), int2(max_texel, max_texel));
            float blocker = shadow_map.Load(int3(texel, 0)).r;
            lit += receiver <= blocker ? 1.0 : 0.0;
        }
    }
    return lit / 9.0;
}

[shader("fragment")]
float4 main(VSOutput input) : SV_Target
{
    DOT_ModelRoot *pc = (DOT_ModelRoot*)g_root.addr;
    DOT_MaterialGpu *mat = pc->material_addr;

    float4 base_color = mat->base_color_factor;
    if((mat->flags & DOT_MaterialFeatureBit_ColorTexture) != 0){
        Texture2D albedo_texture = ResourceDescriptorHeap[mat->tex_slots[DOT_PBRTextureKind_Diffuse]];
        SamplerState albedo_sampler = SamplerDescriptorHeap[mat->sampler_slots[DOT_PBRTextureKind_Diffuse]];
        float4 albedo = albedo_texture.Sample(albedo_sampler, input.texcoord);
        base_color *= albedo;
    }

    DOT_SunLight  *sun    = pc->sun_addr;
    DOT_ShadowGpu *shadow = pc->shadow_addr;
    float3 normal     = normalize(input.normal);
    float  ndotl      = max(dot(normal, normalize(sun->direction)), 0.0);
    float  visibility = ndotl > 0.0 ? sun_shadow(shadow, input.world_pos, normal) : 0.0;
    float3 radiance   = sun->color * sun->energy * ndotl * visibility / 3.14159265 + sun->ambient_color * sun->ambient_energy;

    return float4(base_color.rgb * radiance, base_color.a);
}
