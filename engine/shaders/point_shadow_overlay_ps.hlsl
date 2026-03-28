Texture2D<float> depthTex : register(t0);
TextureCube<float> pointShadowMap : register(t1);
SamplerState pointSampler : register(s0);
SamplerState linearSampler : register(s1);

cbuffer PointShadowOverlayParams : register(b0)
{
    row_major float4x4 invViewProjection;
    float3 lightPosition;
    float lightRange;
    float nearZ;
    float shadowBias;
    float overlayAlpha;
    float _pad0;
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

    float3 lightToFrag = worldPos.xyz - lightPosition;
    float dist = length(lightToFrag);

    // Outside light range = blue tint
    if (dist > lightRange)
        return float4(0.0, 0.0, 0.4, overlayAlpha);

    // Shadow test using dominant axis distance (matches cubemap depth buffer storage)
    float3 absDir = abs(lightToFrag);
    float currentDist = max(absDir.x, max(absDir.y, absDir.z));

    float storedDepth = pointShadowMap.SampleLevel(linearSampler, lightToFrag, 0).r;

    // Reconstruct linear depth from perspective NDC z
    // Perspective LH: z_ndc = far*(d - near) / (d*(far-near))
    // Invert: d = near*far / (far - z_ndc*(far-near))
    float linearDepth = nearZ * lightRange / (lightRange - storedDepth * (lightRange - nearZ));

    float shadow = (currentDist - shadowBias > linearDepth) ? 0.0 : 1.0;

    // Red = fully shadowed (shadow=0), Green = fully lit (shadow=1)
    float3 color = lerp(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), shadow);
    return float4(color, overlayAlpha);
}
