Texture2D<float> depthTex : register(t0);
SamplerState pointSampler : register(s0);

cbuffer DepthHeatmapParams : register(b0)
{
    row_major float4x4 invViewProjection;
    float3 lightPosition;
    float lightRange;
    float overlayAlpha;
    float3 _pad0;
};

struct PSInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    float depth = depthTex.Sample(pointSampler, input.uv);

    // Skip background/sky (depth == 1.0)
    if (depth >= 1.0)
        return float4(0.0, 0.0, 0.0, 0.0);

    // Reconstruct world position from depth buffer
    // UV to NDC: x [0,1] -> [-1,1], y [0,1] -> [1,-1] (Y flip for LH)
    float4 clipPos = float4(input.uv.x * 2.0 - 1.0, -(input.uv.y * 2.0 - 1.0), depth, 1.0);
    float4 worldPos = mul(clipPos, invViewProjection);
    worldPos /= worldPos.w;

    float dist = length(worldPos.xyz - lightPosition);
    float t = dist / lightRange;

    // Outside light range = transparent
    if (t > 1.0)
        return float4(0.0, 0.0, 0.0, 0.0);

    // Heatmap color mapping:
    // Green  (0.0 - 0.5): close to light, good precision
    // Yellow (0.5 - 0.8): mid-range, acceptable precision
    // Red    (0.8 - 0.95): far from light, poor precision
    // Magenta/White (>0.95): danger zone, shadow acne likely
    float3 color;
    if (t < 0.5)
    {
        color = lerp(float3(0.0, 0.8, 0.0), float3(1.0, 1.0, 0.0), t / 0.5);
    }
    else if (t < 0.8)
    {
        color = lerp(float3(1.0, 1.0, 0.0), float3(1.0, 0.0, 0.0), (t - 0.5) / 0.3);
    }
    else if (t < 0.95)
    {
        color = lerp(float3(1.0, 0.0, 0.0), float3(1.0, 0.0, 1.0), (t - 0.8) / 0.15);
    }
    else
    {
        color = lerp(float3(1.0, 0.0, 1.0), float3(1.0, 1.0, 1.0), (t - 0.95) / 0.05);
    }

    return float4(color, overlayAlpha);
}
