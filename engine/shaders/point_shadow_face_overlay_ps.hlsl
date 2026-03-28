Texture2D<float> depthTex : register(t0);
SamplerState pointSampler : register(s0);

cbuffer FaceOverlayParams : register(b0)
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

    float3 lightToFrag = worldPos.xyz - lightPosition;
    float dist = length(lightToFrag);

    // Outside light range = transparent
    if (dist > lightRange)
        return float4(0.0, 0.0, 0.0, 0.0);

    // Determine dominant axis for cubemap face selection
    float3 absDir = abs(lightToFrag);
    float maxAxis = max(absDir.x, max(absDir.y, absDir.z));

    // 6 face colors: +X Red, -X Cyan, +Y Green, -Y Magenta, +Z Blue, -Z Yellow
    float3 color;
    if (maxAxis == absDir.x)
        color = (lightToFrag.x > 0.0) ? float3(1.0, 0.0, 0.0) : float3(0.0, 1.0, 1.0);
    else if (maxAxis == absDir.y)
        color = (lightToFrag.y > 0.0) ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 1.0);
    else
        color = (lightToFrag.z > 0.0) ? float3(0.0, 0.0, 1.0) : float3(1.0, 1.0, 0.0);

    return float4(color, overlayAlpha);
}
