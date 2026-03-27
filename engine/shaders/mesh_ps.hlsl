Texture2D baseColorTex : register(t0);
SamplerState linearSampler : register(s0);

#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS 8

struct PointLight
{
    float3 position;
    float range;
    float3 color;
    float specularPower;
};

struct SpotLight
{
    float3 position;
    float range;
    float3 direction;
    float innerCos;
    float3 color;
    float outerCos;
    float specularPower;
    float3 _pad0;
};

cbuffer PerFrame : register(b0)
{
    row_major float4x4 viewProjection;
    float3 cameraPosition;
    float _pad0;

    float3 lightDirection;
    float lightSpecularPower;
    float3 lightColor;
    float _pad1;
    float3 ambientColor;
    float _pad2;
    int lightingEnabled;
    int numPointLights;
    int numSpotLights;
    float _pad3;

    PointLight pointLights[MAX_POINT_LIGHTS];
    SpotLight spotLights[MAX_SPOT_LIGHTS];
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
        float3 ambient = ambientColor * color;
        float3 diffuse = float3(0, 0, 0);
        float3 specular = float3(0, 0, 0);

        float3 normal = normalize(input.worldNormal);
        float3 viewDir = normalize(cameraPosition - input.worldPos);

        // Directional light
        if (dot(lightColor, lightColor) > 0)
        {
            float3 lightDir = normalize(-lightDirection);
            float3 halfVec = normalize(lightDir + viewDir);

            float nDotL = max(dot(normal, lightDir), 0.0);
            diffuse = nDotL * lightColor * color;

            float nDotH = max(dot(normal, halfVec), 0.0);
            float spec = pow(nDotH, lightSpecularPower);
            specular = spec * lightColor;
        }

        // Point lights
        for (int i = 0; i < numPointLights; i++)
        {
            float3 toLight = pointLights[i].position - input.worldPos;
            float dist = length(toLight);
            float3 plDir = toLight / max(dist, 0.0001);

            float attenuation = saturate(1.0 - dist / pointLights[i].range);
            attenuation *= attenuation;

            float3 halfVec = normalize(plDir + viewDir);

            float nDotL = max(dot(normal, plDir), 0.0);
            diffuse += nDotL * pointLights[i].color * color * attenuation;

            float nDotH = max(dot(normal, halfVec), 0.0);
            float spec = pow(nDotH, pointLights[i].specularPower);
            specular += spec * pointLights[i].color * attenuation;
        }

        // Spot lights
        for (int j = 0; j < numSpotLights; j++)
        {
            float3 toLight = spotLights[j].position - input.worldPos;
            float dist = length(toLight);
            float3 slDir = toLight / max(dist, 0.0001);

            float distAtten = saturate(1.0 - dist / spotLights[j].range);
            distAtten *= distAtten;

            float cosAngle = dot(spotLights[j].direction, -slDir);
            float denom = spotLights[j].innerCos - spotLights[j].outerCos;
            float spotAtten = saturate((cosAngle - spotLights[j].outerCos) / (abs(denom) < 0.0001 ? 0.0001 : denom));

            float atten = distAtten * spotAtten;

            float3 halfVec = normalize(slDir + viewDir);

            float nDotL = max(dot(normal, slDir), 0.0);
            diffuse += nDotL * spotLights[j].color * color * atten;

            float nDotH = max(dot(normal, halfVec), 0.0);
            float spec = pow(nDotH, spotLights[j].specularPower);
            specular += spec * spotLights[j].color * atten;
        }

        color = ambient + diffuse + specular;
    }

    if (primitiveHighlight > 0.0)
    {
        float3 highlight = float3(1.0, 0.6, 0.1);
        color = lerp(color, color + highlight * 0.4, primitiveHighlight);
    }

    return float4(color, baseColor.a);
}
