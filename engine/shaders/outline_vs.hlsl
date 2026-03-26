struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VSOutput main(uint vertexId : SV_VertexID)
{
    // Fullscreen triangle from 3 vertices (no vertex buffer needed)
    // vertexId 0 -> (-1, -1), 1 -> (3, -1), 2 -> (-1, 3)
    VSOutput output;
    output.uv = float2((vertexId << 1) & 2, vertexId & 2);
    output.position = float4(output.uv * 2.0 - 1.0, 0.0, 1.0);
    output.uv.y = 1.0 - output.uv.y; // flip Y for texture coordinates
    return output;
}
