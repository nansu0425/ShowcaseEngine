cbuffer OutlineParams : register(b0)
{
    uint selectedObjectId;
    float2 texelSize;
    float _pad0;
    float4 outlineColor;
};

Texture2D<uint> objectIdTex : register(t0);

struct PSInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    int2 pixel = int2(input.position.xy);
    uint centerId = objectIdTex.Load(int3(pixel, 0));
    bool centerIsSelected = (centerId == selectedObjectId);

    // Check neighbors in a 2-pixel radius for edges
    static const int RADIUS = 2;

    for (int y = -RADIUS; y <= RADIUS; y++)
    {
        for (int x = -RADIUS; x <= RADIUS; x++)
        {
            if (x == 0 && y == 0)
                continue;

            uint neighborId = objectIdTex.Load(int3(pixel + int2(x, y), 0));
            bool neighborIsSelected = (neighborId == selectedObjectId);

            if (centerIsSelected != neighborIsSelected)
            {
                return outlineColor;
            }
        }
    }

    return float4(0.0, 0.0, 0.0, 0.0);
}
