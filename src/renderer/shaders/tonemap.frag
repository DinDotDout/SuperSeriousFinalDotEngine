#include "renderer/shaders/shader_shared.h"
#include "renderer/shaders/tonemap_shared.h"

struct VSOutput {
    float4 sv_position : SV_Position;
    float2 uv          : TEXCOORD0;
};

float3 aces(float3 v)
{
    float3x3 input_matrix = float3x3(
        float3(0.59719, 0.07600, 0.02840),
        float3(0.35458, 0.90834, 0.13383),
        float3(0.04823, 0.01566, 0.83777));
    float3x3 output_matrix = float3x3(
        float3( 1.60475, -0.10208, -0.00327),
        float3(-0.53108,  1.10813, -0.07276),
        float3(-0.07367, -0.00605,  1.07602));
    v = mul(v, input_matrix);
    float3 a = v * (v + 0.0245786) - 0.000090537;
    float3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    v = a / b;
    return mul(v, output_matrix);
}

[shader("fragment")]
float4 main(VSOutput input) : SV_Target
{
    RN_TonemapRoot *r = (RN_TonemapRoot*)g_root.addr;
    Texture2D hdr_texture = ResourceDescriptorHeap[r->hdr_slot];
    float3 hdr = hdr_texture.Load(int3(int2(input.sv_position.xy), 0)).rgb;
    if(r->bypass != 0){
        return float4(hdr, 1.0);
    }
    return float4(saturate(aces(hdr * r->exposure)), 1.0);
}
