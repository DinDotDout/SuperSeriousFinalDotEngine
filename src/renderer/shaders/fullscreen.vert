#include "renderer/shaders/shader_shared.h"

struct VSOutput {
    float4 sv_position : SV_Position;
    float2 uv          : TEXCOORD0;
};

[shader("vertex")]
VSOutput main(uint vertex_index : SV_VertexID)
{
    float2 uv = float2((vertex_index << 1) & 2, vertex_index & 2);

    VSOutput out;
    out.sv_position = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    out.uv          = uv;
    return out;
}
