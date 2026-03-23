cbuffer PerFrame : register(b0) {
    row_major float4x4 viewProjection;
    float3 cameraPosition;
    float _pad0;
    float gridFadeStart;
    float gridFadeEnd;
    float gridOpacity;
    float _pad1;
};

cbuffer PerObject : register(b1) {
    row_major float4x4 world;
};

struct VSInput {
    float3 position : POSITION;
    float4 color : COLOR;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float3 worldPos : TEXCOORD0;
};

VSOutput main(VSInput input) {
    VSOutput output;

    float4 worldPos = mul(float4(input.position, 1.0), world);
    output.position = mul(worldPos, viewProjection);
    output.worldPos = worldPos.xyz;
    output.color = input.color;
    return output;
}
