#include "depth_utils.hlsli"

TextureCube<float> cubemapDepth : register(t0);
SamplerState pointSampler : register(s0);

cbuffer PreviewConstants : register(b0)
{
    uint faceIndex; // 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
    float nearZ;    // 0.1
    float farZ;     // light range
    uint _pad;
};

struct PSInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    // Map UV [0,1] to face-local coordinates [-1,+1]
    float u = input.uv.x * 2.0 - 1.0;
    float v = -(input.uv.y * 2.0 - 1.0); // flip Y: UV.y=0 is top, cubemap +Y is up

    // Build direction vector for the requested cubemap face (D3D12 face order)
    float3 dir;
    switch (faceIndex)
    {
    case 0:
        dir = float3(+1.0, v, -u);
        break; // +X
    case 1:
        dir = float3(-1.0, v, +u);
        break; // -X
    case 2:
        dir = float3(u, +1.0, -v);
        break; // +Y
    case 3:
        dir = float3(u, -1.0, +v);
        break; // -Y
    case 4:
        dir = float3(u, v, +1.0);
        break; // +Z
    case 5:
        dir = float3(-u, v, -1.0);
        break; // -Z
    default:
        dir = float3(0, 0, 1);
        break;
    }

    float storedDepth = cubemapDepth.SampleLevel(pointSampler, normalize(dir), 0).r;

    // Linearize perspective depth (matches CalculatePointShadow in mesh_ps.hlsl)
    float linearDepth = (nearZ * farZ) / (farZ - storedDepth * (farZ - nearZ));
    float normalized = linearDepth / farZ;

    return DepthToGrayscale(normalized);
}
