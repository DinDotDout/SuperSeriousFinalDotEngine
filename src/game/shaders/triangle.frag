struct VSOutput
{
    float4 sv_position : SV_Position;
    float3 color : COLOR0;
};

[shader("fragment")]
float4 main(VSOutput input) : SV_Target
{
    return float4(input.color, 1.0);
}
