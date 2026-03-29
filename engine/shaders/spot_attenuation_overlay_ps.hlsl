Texture2D<float> depthTex : register(t0);
SamplerState pointSampler : register(s0);

cbuffer SpotAttenuationOverlayParams : register(b0)
{
    row_major float4x4 invViewProjection;
    float3 lightPosition;
    float lightRange;
    float3 lightDirection;
    float outerCos;
    float innerCos;
    float overlayAlpha;
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
    float3 lightToFragNorm = lightToFrag / max(dist, 0.0001);

    // Distance attenuation: squared falloff (matches mesh_ps.hlsl)
    float distAtten = saturate(1.0 - dist / lightRange);
    distAtten *= distAtten;

    // Angular (cone) attenuation (matches mesh_ps.hlsl)
    float cosAngle = dot(lightDirection, lightToFragNorm);
    float denom = innerCos - outerCos;
    float spotAtten = saturate((cosAngle - outerCos) / (abs(denom) < 0.0001 ? 0.0001 : denom));

    // Combined attenuation
    float atten = distAtten * spotAtten;

    // Outside cone/range entirely -> transparent
    if (atten <= 0.0)
        return float4(0.0, 0.0, 0.0, 0.0);

    // Heatmap: green (high) -> yellow (mid) -> red (low)
    float3 color;
    if (atten >= 0.5)
    {
        color = float3(0.0, 1.0, 0.0);
    }
    else if (atten >= 0.2)
    {
        float t = (atten - 0.2) / 0.3;
        color = lerp(float3(1.0, 1.0, 0.0), float3(0.0, 1.0, 0.0), t);
    }
    else
    {
        float t = (atten - 0.01) / 0.19;
        color = lerp(float3(1.0, 0.0, 0.0), float3(1.0, 1.0, 0.0), saturate(t));
    }

    return float4(color, overlayAlpha);
}
