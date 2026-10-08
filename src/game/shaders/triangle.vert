#include "renderer/shaders/shader_shared.h"
#include "game/shaders/triangle_shared.h"

struct VSOutput {
    float4 sv_position : SV_Position;
    float3 color : COLOR0;
};

[shader("vertex")]
VSOutput main(uint vertex_index : SV_VertexID)
{
    TriangleRoot *pc = (TriangleRoot*)g_root.addr;
    TriangleVertex *verts = (TriangleVertex*)pc->vertex_addr;
    TriangleVertex v = verts[vertex_index];

    VSOutput out;
    out.sv_position = mul(pc->mvp, float4(v.position, 1.0));
    out.color = v.color;
    return out;
}
