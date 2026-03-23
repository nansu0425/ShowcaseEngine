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
    float3 worldNorm : NORMAL;
    float3 worldPos : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    float4 worldPos = mul(float4(input.position, 1.0), world);
    output.position = mul(worldPos, viewProjection);
    output.worldPos = worldPos.xyz;
    output.worldNorm = normalize(mul(input.normal, (float3x3)world));
    output.texCoord = input.texCoord;

    return output;
}
