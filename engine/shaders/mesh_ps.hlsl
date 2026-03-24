Texture2D baseColorTex : register(t0);
SamplerState linearSampler : register(s0);

cbuffer PerMaterial : register(b2)
{
    float4 baseColorFactor;
    int hasTexture;
    float selectionTint;
    float2 _pad1;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    float4 baseColor = baseColorFactor;
    if (hasTexture)
    {
        baseColor *= baseColorTex.Sample(linearSampler, input.texCoord);
    }

    float3 color = baseColor.rgb;

    if (selectionTint > 0.0)
    {
        float3 highlight = float3(0.2, 0.5, 1.0);
        color = lerp(color, color + highlight * 0.3, selectionTint);
    }

    return float4(color, baseColor.a);
}
