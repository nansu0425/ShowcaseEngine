Texture2D baseColorTex : register(t0);
SamplerState linearSampler : register(s0);

cbuffer PerMaterial : register(b2)
{
    float4 baseColorFactor;
    int hasTexture;
    float selectionTint;
    float alphaCutoff;
    int alphaMode;
};

cbuffer ObjectIdConstants : register(b3)
{
    uint objectId;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

uint main(PSInput input) : SV_TARGET
{
    if (alphaMode == 1)
    {
        float alpha = baseColorFactor.a;
        if (hasTexture)
        {
            alpha *= baseColorTex.Sample(linearSampler, input.texCoord).a;
        }
        clip(alpha - alphaCutoff);
    }

    return objectId;
}
