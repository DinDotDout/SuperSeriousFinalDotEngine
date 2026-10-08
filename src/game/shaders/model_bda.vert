#include "renderer/shaders/shader_shared.h"
#include "game/shaders/model_shared.h"

struct VSOutput {
    float4 sv_position : SV_Position;
    float3 normal   : NORMAL0;
    float2 texcoord : TEXCOORD0;
};

[shader("vertex")]
VSOutput main(uint vertex_index : SV_VertexID)
{
    DOT_ModelRoot *pc = (DOT_ModelRoot*)g_root.addr;
    uint idx = vertex_index;

    float3 *positions = (float3*)pc->position_addr;
    float3 *normals   = (float3*)pc->normal_addr;
    float2 *texcoords = (float2*)pc->texcoord_addr;

    VSOutput out;
    out.sv_position = mul(pc->mvp, float4(positions[idx], 1.0));
    out.normal      = normals[idx];
    out.texcoord    = texcoords[idx];
    return out;
}
