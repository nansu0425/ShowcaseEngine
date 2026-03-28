Texture2D<float> depthTex : register(t0);
Texture2D shadowMap : register(t1);
SamplerState pointSampler : register(s0);
SamplerComparisonState shadowSampler : register(s1);

cbuffer ShadowOverlayParams : register(b0)
{
    row_major float4x4 invViewProjection;
    row_major float4x4 lightViewProjection;
    float shadowBias;
    float shadowMapTexelSize;
    float overlayAlpha;
    float _pad0;
};

struct PSInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float CalculateShadow(float3 worldPos)
{
    float4 lightClipPos = mul(float4(worldPos, 1.0), lightViewProjection);
    float3 projCoords = lightClipPos.xyz / lightClipPos.w;

    // NDC to UV (LH: x [-1,1] -> [0,1], y [-1,1] -> [1,0])
    float2 shadowUV;
    shadowUV.x = projCoords.x * 0.5 + 0.5;
    shadowUV.y = -projCoords.y * 0.5 + 0.5;

    float currentDepth = projCoords.z;

    // Outside shadow map bounds = no shadow
    if (shadowUV.x < 0.0 || shadowUV.x > 1.0 || shadowUV.y < 0.0 || shadowUV.y > 1.0 || currentDepth > 1.0 ||
        currentDepth < 0.0)
        return 1.0;

    // 3x3 PCF
    float shadow = 0.0;
    [unroll] for (int x = -1; x <= 1; x++)
    {
        [unroll] for (int y = -1; y <= 1; y++)
        {
            float2 offset = float2(x, y) * shadowMapTexelSize;
            shadow += shadowMap.SampleCmpLevelZero(shadowSampler, shadowUV + offset, currentDepth - shadowBias);
        }
    }
    return shadow / 9.0;
}

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

    float shadow = CalculateShadow(worldPos.xyz);

    // Red = fully shadowed (shadow=0), Green = fully lit (shadow=1)
    float3 color = lerp(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), shadow);
    return float4(color, overlayAlpha);
}
