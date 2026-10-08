#include "renderer/shaders/shader_shared.h"
#include "renderer/shaders/ui_shared.h"

struct VSOutput {
    float4 sv_position : SV_Position;
    float2 uv          : TEXCOORD0;
    float4 color       : COLOR0;
};

[shader("fragment")]
float4 main(VSOutput input) : SV_Target
{
    UI_RenderRoot *pc = (UI_RenderRoot*)g_root.addr;
    Texture2D texture = ResourceDescriptorHeap[pc->texture_slot];
    SamplerState sampler = SamplerDescriptorHeap[pc->sampler_slot];
    return input.color * texture.Sample(sampler, input.uv);
}
