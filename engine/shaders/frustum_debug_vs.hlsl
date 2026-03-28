cbuffer FrustumConstants : register(b0)
{
    row_major float4x4 viewProjection; // 16 DWORDs
    float4 color;                      // 4 DWORDs
    float4 corners[8];                 // 32 DWORDs (.xyz = world position, .w = padding)
    uint mode;                         // 1 DWORD (0 = triangles, 1 = lines)
};

struct VSOutput
{
    float4 position : SV_Position;
    float4 color : COLOR0;
};

// 6 faces x 2 triangles x 3 vertices = 36
static const uint TRI_INDICES[36] = {
    0, 2, 1, 0, 3, 2, // near
    4, 5, 6, 4, 6, 7, // far
    0, 7, 3, 0, 4, 7, // left
    1, 2, 6, 1, 6, 5, // right
    3, 6, 2, 3, 7, 6, // top
    0, 1, 5, 0, 5, 4, // bottom
};

// 12 edges x 2 vertices = 24
static const uint LINE_INDICES[24] = {
    0, 1, 1, 2, 2, 3, 3, 0, // near
    4, 5, 5, 6, 6, 7, 7, 4, // far
    0, 4, 1, 5, 2, 6, 3, 7, // connecting
};

VSOutput main(uint vertexId : SV_VertexID)
{
    VSOutput output;

    uint cornerIdx = (mode == 0) ? TRI_INDICES[vertexId] : LINE_INDICES[vertexId];
    float3 worldPos = corners[cornerIdx].xyz;

    output.position = mul(float4(worldPos, 1.0), viewProjection);
    output.color = color;
    return output;
}
