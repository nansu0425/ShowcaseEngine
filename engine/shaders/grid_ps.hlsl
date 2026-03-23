cbuffer PerFrame : register(b0) {
    row_major float4x4 viewProjection;
    float3 cameraPosition;
    float _pad0;
    float gridFadeStart;
    float gridFadeEnd;
    float gridOpacity;
    float _pad1;
};

struct PSInput {
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float3 worldPos : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET {
    float dist = length(input.worldPos - cameraPosition);
    float fadeRange = max(gridFadeEnd - gridFadeStart, 1e-4);
    float fade = 1.0 - saturate((dist - gridFadeStart) / fadeRange);

    // Axis lines pass vertex color through (alpha > 0)
    if (input.color.a > 0.0) {
        float4 output = input.color;
        output.a *= fade * gridOpacity;
        return output;
    }

    // Procedural grid: Pristine Grid technique (Moiré-free)
    float2 coord = input.worldPos.xz;
    float lineWidth = 0.04;

    // Per-axis screen-space derivatives
    float2 uvDDX = ddx(coord);
    float2 uvDDY = ddy(coord);
    float2 uvDeriv = float2(length(float2(uvDDX.x, uvDDY.x)), length(float2(uvDDX.y, uvDDY.y)));

    // Clamp draw width: at least derivative size (prevents sub-pixel aliasing)
    float2 drawWidth = clamp(float2(lineWidth, lineWidth), uvDeriv, 0.5);
    float2 lineAA = uvDeriv * 1.5;

    // Grid pattern with smoothstep anti-aliasing
    float2 gridUV = 1.0 - abs(frac(coord) * 2.0 - 1.0);
    float2 grid2 = smoothstep(drawWidth + lineAA, drawWidth - lineAA, gridUV);

    // Scale by target/draw ratio (compensate for clamped width)
    grid2 *= saturate(lineWidth / drawWidth);

    // Fade to average coverage when derivatives exceed Nyquist limit
    grid2 = lerp(grid2, float2(lineWidth, lineWidth), saturate(uvDeriv * 2.0 - 1.0));

    // Combine both axes
    float gridAlpha = lerp(grid2.x, 1.0, grid2.y);

    float alpha = gridAlpha * fade * gridOpacity;
    return float4(0.35, 0.35, 0.35, alpha);
}
