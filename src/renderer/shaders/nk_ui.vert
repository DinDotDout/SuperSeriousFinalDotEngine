// Nuklear vertex: float2 position, float2 uv, rgba8 color = 5 words, read raw so no struct layout
// rules apply.
#include "renderer/shaders/shader_shared.h"
#include "renderer/shaders/ui_shared.h"

struct VSOutput {
    float4 sv_position : SV_Position;
    float2 uv          : TEXCOORD0;
    float4 color       : COLOR0;
};

float3 srgb_to_linear(float3 c)
{
    return select(c <= 0.04045, c / 12.92, pow((c + 0.055) / 1.055, 2.4));
}

[shader("vertex")]
VSOutput main(uint vertex_index : SV_VertexID)
{
    UI_RenderRoot *pc = (UI_RenderRoot*)g_root.addr;
    uint *words = (uint*)pc->vertex_addr;
    uint base = vertex_index * 5;

    float2 position = float2(asfloat(words[base + 0]), asfloat(words[base + 1]));
    float2 uv       = float2(asfloat(words[base + 2]), asfloat(words[base + 3]));
    uint   packed   = words[base + 4];
    float4 color    = float4(packed & 0xff, (packed >> 8) & 0xff, (packed >> 16) & 0xff, packed >> 24) / 255.0;

    VSOutput out;
    out.sv_position = float4(position * pc->scale + pc->translate, 0.0, 1.0);
    out.uv          = uv;
    // Nuklear colors are sRGB; the swapchain encodes on write.
    out.color       = float4(srgb_to_linear(color.rgb), color.a);
    return out;
}
