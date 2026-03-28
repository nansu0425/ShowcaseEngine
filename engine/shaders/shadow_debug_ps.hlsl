Texture2D<float> depthTex : register(t0);
SamplerState pointSampler : register(s0);

struct PSInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    float depth = depthTex.Sample(pointSampler, input.uv);
    // Orthographic projection depth is already linear [0,1] — display directly
    return float4(depth, depth, depth, 1.0);
}
