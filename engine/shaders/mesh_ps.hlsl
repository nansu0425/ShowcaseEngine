Texture2D baseColorTex : register(t0);
SamplerState linearSampler : register(s0);

cbuffer PerMaterial : register(b2)
{
    float4 baseColorFactor;
    int hasTexture;
    float selectionTint;
    float alphaCutoff;
    int alphaMode;
    float primitiveHighlight;
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

    if (alphaMode == 1)
    {
        clip(baseColor.a - alphaCutoff);
    }

    float3 color = baseColor.rgb;

    if (primitiveHighlight > 0.0)
    {
        float3 highlight = float3(1.0, 0.6, 0.1);
        color = lerp(color, color + highlight * 0.4, primitiveHighlight);
    }

    return float4(color, baseColor.a);
}
