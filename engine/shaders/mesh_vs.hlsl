cbuffer PerFrame : register(b0)
{
    row_major float4x4 viewProjection;
    float3 cameraPosition;
    float _pad0;
};

cbuffer PerObject : register(b1)
{
    row_major float4x4 world;
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
    float3 worldNormal : TEXCOORD2;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    float4 worldPos = mul(float4(input.position, 1.0), world);
    output.position = mul(worldPos, viewProjection);
    output.texCoord = input.texCoord;
    output.worldPos = worldPos.xyz;

    // Transform normal using cofactor of 3x3 world matrix
    // Correct for non-uniform scale without CPU-side inverse-transpose
    float3 col0 = float3(world[0][0], world[0][1], world[0][2]);
    float3 col1 = float3(world[1][0], world[1][1], world[1][2]);
    float3 col2 = float3(world[2][0], world[2][1], world[2][2]);
    float3x3 cofactor = float3x3(cross(col1, col2), cross(col2, col0), cross(col0, col1));
    output.worldNormal = normalize(mul(input.normal, cofactor));

    return output;
}
