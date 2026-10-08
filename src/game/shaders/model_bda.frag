#include "renderer/shaders/shader_shared.h"
#include "game/shaders/model_shared.h"

struct VSOutput {
    float4 sv_position : SV_Position;
    float3 normal : NORMAL0;
    float2 texcoord : TEXCOORD0;
};

[shader("fragment")]
float4 main(VSOutput input) : SV_Target
{
    DOT_ModelRoot *pc = (DOT_ModelRoot*)g_root.addr;
    DOT_MaterialGpu *mat = (DOT_MaterialGpu*)pc->material_addr;

    float4 base_color = mat->base_color_factor;
    if((mat->flags & DOT_MaterialFeatureBit_ColorTexture) != 0){
        Texture2D albedo_texture = ResourceDescriptorHeap[mat->tex_slots[DOT_PBRTextureKind_Diffuse]];
        SamplerState albedo_sampler = SamplerDescriptorHeap[mat->sampler_slots[DOT_PBRTextureKind_Diffuse]];
        float4 albedo = albedo_texture.Sample(albedo_sampler, input.texcoord);
        base_color *= albedo;
    }

    float3 light_dir = normalize(float3(0.5, 1.0, 0.3));
    float ndotl = max(dot(normalize(input.normal), light_dir), 0.2); // 0.2 floor as crude ambient

    return float4(base_color.rgb * ndotl, base_color.a);
}
