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
    float3 worldNorm : NORMAL;
    float3 worldPos : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
};

static const float3 LIGHT_DIR = normalize(float3(0.4, -0.8, 0.3));
static const float3 LIGHT_COLOR = float3(1.0, 0.98, 0.95);
static const float AMBIENT = 0.15;

float4 main(PSInput input) : SV_TARGET
{
    float3 normal = normalize(input.worldNorm);
    float ndl = saturate(dot(normal, -LIGHT_DIR));
    float3 diffuse = LIGHT_COLOR * ndl;

    float4 baseColor = baseColorFactor;
    if (hasTexture)
    {
        baseColor *= baseColorTex.Sample(linearSampler, input.texCoord);
    }

    float3 color = baseColor.rgb * (diffuse + AMBIENT);

    if (selectionTint > 0.0)
    {
        float3 highlight = float3(0.2, 0.5, 1.0);
        color = lerp(color, color + highlight * 0.3, selectionTint);
    }

    return float4(color, baseColor.a);
}
