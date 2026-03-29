Texture2D baseColorTex : register(t0);
Texture2D shadowMap : register(t1);
TextureCube pointShadowMap0 : register(t2);
TextureCube pointShadowMap1 : register(t3);
TextureCube pointShadowMap2 : register(t4);
TextureCube pointShadowMap3 : register(t5);
TextureCube pointShadowMap4 : register(t6);
TextureCube pointShadowMap5 : register(t7);
Texture2D spotShadowMap0 : register(t8);
Texture2D spotShadowMap1 : register(t9);
Texture2D spotShadowMap2 : register(t10);
Texture2D spotShadowMap3 : register(t11);
Texture2D spotShadowMap4 : register(t12);
Texture2D spotShadowMap5 : register(t13);
Texture2D spotShadowMap6 : register(t14);
Texture2D spotShadowMap7 : register(t15);
SamplerState linearSampler : register(s0);
SamplerComparisonState shadowSampler : register(s1);

#define MAX_POINT_LIGHTS 6
#define MAX_SPOT_LIGHTS 8

struct PointLight
{
    float3 position;
    float range;
    float3 color;
    float specularPower;
    int shadowIndex;
    float shadowBias;
    int enablePCF;
    float _pad1;
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
    int shadowIndex;
    float shadowBias;
    int enablePCF;
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

    // Shadow mapping (directional)
    row_major float4x4 lightViewProjection;
    int shadowEnabled;
    float shadowBias;
    float shadowMapTexelSize;
    int dirPcfEnabled;

    // Point light shadow mapping
    int numPointShadowLights;
    float pointShadowNearZ;
    float pointShadowMapResolution;
    float _pad5;

    // Spot light shadow mapping
    int numSpotShadowLights;
    float spotShadowNearZ;
    float2 _pad6;
    row_major float4x4 spotShadowVP[MAX_SPOT_LIGHTS];
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

float CalculateShadow(float3 worldPos, int pcfEnabled)
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

    if (pcfEnabled)
    {
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
    else
    {
        return shadowMap.SampleCmpLevelZero(shadowSampler, shadowUV, currentDepth - shadowBias);
    }
}

float SamplePointShadowCmp(float3 dir, float compareDepth, int idx)
{
    if (idx == 0)
        return pointShadowMap0.SampleCmpLevelZero(shadowSampler, dir, compareDepth);
    if (idx == 1)
        return pointShadowMap1.SampleCmpLevelZero(shadowSampler, dir, compareDepth);
    if (idx == 2)
        return pointShadowMap2.SampleCmpLevelZero(shadowSampler, dir, compareDepth);
    if (idx == 3)
        return pointShadowMap3.SampleCmpLevelZero(shadowSampler, dir, compareDepth);
    if (idx == 4)
        return pointShadowMap4.SampleCmpLevelZero(shadowSampler, dir, compareDepth);
    return pointShadowMap5.SampleCmpLevelZero(shadowSampler, dir, compareDepth);
}

void GetCubemapFaceTangents(float3 dir, out float3 tangent, out float3 bitangent)
{
    float3 absDir = abs(dir);
    if (absDir.x >= absDir.y && absDir.x >= absDir.z)
    {
        // +X or -X face
        tangent = float3(0, 0, -sign(dir.x));
        bitangent = float3(0, -1, 0);
    }
    else if (absDir.y >= absDir.x && absDir.y >= absDir.z)
    {
        // +Y or -Y face
        tangent = float3(1, 0, 0);
        bitangent = float3(0, 0, sign(dir.y));
    }
    else
    {
        // +Z or -Z face
        tangent = float3(sign(dir.z), 0, 0);
        bitangent = float3(0, -1, 0);
    }
}

float CalculatePointShadow(float3 worldPos, float3 lightPos, float lightRange, float bias, int shadowIdx,
                           int pcfEnabled)
{
    float3 lightToFrag = worldPos - lightPos;
    // Dominant axis distance matches what the perspective depth buffer stores per cubemap face
    float3 absDir = abs(lightToFrag);
    float currentDist = max(absDir.x, max(absDir.y, absDir.z));

    // Convert dominant-axis distance to NDC z (matches stored depth buffer values)
    // Perspective LH: z_ndc = far*(d - near) / (d*(far-near))
    float nearZ = pointShadowNearZ;
    float farZ = lightRange;
    float ndcZ = farZ * (currentDist - nearZ) / (currentDist * (farZ - nearZ));
    float compareValue = ndcZ - bias;

    if (pcfEnabled)
    {
        // 3x3 PCF: offset sample direction in the dominant face's tangent plane
        float texelSize = 2.0 * currentDist / pointShadowMapResolution;
        float3 tangent, bitangent;
        GetCubemapFaceTangents(lightToFrag, tangent, bitangent);

        float shadow = 0.0;
        [unroll] for (int x = -1; x <= 1; x++)
        {
            [unroll] for (int y = -1; y <= 1; y++)
            {
                float3 offsetDir = lightToFrag + (tangent * x + bitangent * y) * texelSize;
                shadow += SamplePointShadowCmp(offsetDir, compareValue, shadowIdx);
            }
        }
        return shadow / 9.0;
    }
    else
    {
        return SamplePointShadowCmp(lightToFrag, compareValue, shadowIdx);
    }
}

float SampleSpotShadowCmp(float2 uv, float compareDepth, int idx)
{
    if (idx == 0)
        return spotShadowMap0.SampleCmpLevelZero(shadowSampler, uv, compareDepth);
    if (idx == 1)
        return spotShadowMap1.SampleCmpLevelZero(shadowSampler, uv, compareDepth);
    if (idx == 2)
        return spotShadowMap2.SampleCmpLevelZero(shadowSampler, uv, compareDepth);
    if (idx == 3)
        return spotShadowMap3.SampleCmpLevelZero(shadowSampler, uv, compareDepth);
    if (idx == 4)
        return spotShadowMap4.SampleCmpLevelZero(shadowSampler, uv, compareDepth);
    if (idx == 5)
        return spotShadowMap5.SampleCmpLevelZero(shadowSampler, uv, compareDepth);
    if (idx == 6)
        return spotShadowMap6.SampleCmpLevelZero(shadowSampler, uv, compareDepth);
    return spotShadowMap7.SampleCmpLevelZero(shadowSampler, uv, compareDepth);
}

float CalculateSpotShadow(float3 worldPos, int shadowIdx, float bias, int pcfEnabled)
{
    float4 lightClipPos = mul(float4(worldPos, 1.0), spotShadowVP[shadowIdx]);
    float3 projCoords = lightClipPos.xyz / lightClipPos.w;

    float2 shadowUV;
    shadowUV.x = projCoords.x * 0.5 + 0.5;
    shadowUV.y = -projCoords.y * 0.5 + 0.5;

    float currentDepth = projCoords.z;

    if (shadowUV.x < 0.0 || shadowUV.x > 1.0 || shadowUV.y < 0.0 || shadowUV.y > 1.0 || currentDepth > 1.0 ||
        currentDepth < 0.0)
        return 1.0;

    if (pcfEnabled)
    {
        // 3x3 PCF
        float texelSize = 1.0 / 1024.0;
        float shadow = 0.0;
        [unroll] for (int x = -1; x <= 1; x++)
        {
            [unroll] for (int y = -1; y <= 1; y++)
            {
                float2 offset = float2(x, y) * texelSize;
                shadow += SampleSpotShadowCmp(shadowUV + offset, currentDepth - bias, shadowIdx);
            }
        }
        return shadow / 9.0;
    }
    else
    {
        return SampleSpotShadowCmp(shadowUV, currentDepth - bias, shadowIdx);
    }
}

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
            float shadow = 1.0;
            if (shadowEnabled)
                shadow = CalculateShadow(input.worldPos, dirPcfEnabled);

            float3 lightDir = normalize(-lightDirection);
            float3 halfVec = normalize(lightDir + viewDir);

            float nDotL = max(dot(normal, lightDir), 0.0);
            diffuse = nDotL * lightColor * color * shadow;

            float nDotH = max(dot(normal, halfVec), 0.0);
            float spec = pow(nDotH, lightSpecularPower);
            specular = spec * lightColor * shadow;
        }

        // Point lights
        for (int i = 0; i < numPointLights; i++)
        {
            float3 toLight = pointLights[i].position - input.worldPos;
            float dist = length(toLight);
            float3 plDir = toLight / max(dist, 0.0001);

            float attenuation = saturate(1.0 - dist / pointLights[i].range);
            attenuation *= attenuation;

            float shadow = 1.0;
            if (pointLights[i].shadowIndex >= 0)
            {
                shadow = CalculatePointShadow(input.worldPos, pointLights[i].position, pointLights[i].range,
                                              pointLights[i].shadowBias, pointLights[i].shadowIndex,
                                              pointLights[i].enablePCF);
            }

            float3 halfVec = normalize(plDir + viewDir);

            float nDotL = max(dot(normal, plDir), 0.0);
            diffuse += nDotL * pointLights[i].color * color * attenuation * shadow;

            float nDotH = max(dot(normal, halfVec), 0.0);
            float spec = pow(nDotH, pointLights[i].specularPower);
            specular += spec * pointLights[i].color * attenuation * shadow;
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

            float shadow = 1.0;
            if (spotLights[j].shadowIndex >= 0)
            {
                shadow = CalculateSpotShadow(input.worldPos, spotLights[j].shadowIndex, spotLights[j].shadowBias,
                                             spotLights[j].enablePCF);
            }

            float3 halfVec = normalize(slDir + viewDir);

            float nDotL = max(dot(normal, slDir), 0.0);
            diffuse += nDotL * spotLights[j].color * color * atten * shadow;

            float nDotH = max(dot(normal, halfVec), 0.0);
            float spec = pow(nDotH, spotLights[j].specularPower);
            specular += spec * spotLights[j].color * atten * shadow;
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
