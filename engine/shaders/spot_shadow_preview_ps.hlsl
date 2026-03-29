Texture2D<float> depthTex : register(t0);
SamplerState pointSampler : register(s0);

cbuffer SpotPreviewConstants : register(b0)
{
    float nearZ; // 0.1
    float farZ;  // light range
    float _pad0;
    float _pad1;
};

struct PSInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    float storedDepth = depthTex.Sample(pointSampler, input.uv);

    // Linearize perspective depth (matches CalculateSpotShadow in mesh_ps.hlsl)
    float linearDepth = (nearZ * farZ) / (farZ - storedDepth * (farZ - nearZ));
    float normalized = 1.0 - linearDepth / farZ;

    return float4(normalized, normalized, normalized, 1.0);
}
