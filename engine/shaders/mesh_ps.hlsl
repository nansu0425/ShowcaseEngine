Texture2D baseColorTex : register(t0);
SamplerState linearSampler : register(s0);

cbuffer PerFrame : register(b0)
{
    row_major float4x4 viewProjection;
    float3 cameraPosition;
    float _pad0;

    float3 lightDirection;
    float lightAmbient;
    float3 lightColor;
    float lightSpecularPower;
    int lightingEnabled;
    float3 _pad1;
};

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
    float3 worldPos : TEXCOORD1;
    float3 worldNormal : TEXCOORD2;
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

    if (lightingEnabled)
    {
        float3 normal = normalize(input.worldNormal);
        float3 lightDir = normalize(-lightDirection);
        float3 viewDir = normalize(cameraPosition - input.worldPos);
        float3 halfVec = normalize(lightDir + viewDir);

        float3 ambient = lightAmbient * color;

        float nDotL = max(dot(normal, lightDir), 0.0);
        float3 diffuse = nDotL * lightColor * color;

        float nDotH = max(dot(normal, halfVec), 0.0);
        float spec = pow(nDotH, lightSpecularPower);
        float3 specular = spec * lightColor;

        color = ambient + diffuse + specular;
    }

    if (primitiveHighlight > 0.0)
    {
        float3 highlight = float3(1.0, 0.6, 0.1);
        color = lerp(color, color + highlight * 0.4, primitiveHighlight);
    }

    return float4(color, baseColor.a);
}
