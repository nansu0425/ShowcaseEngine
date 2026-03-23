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

    // Axis detection: color grid lines at x=0 (Z-axis) and z=0 (X-axis)
    float2 axisUV = abs(coord);
    float2 axisGrid = smoothstep(drawWidth + lineAA, drawWidth - lineAA, axisUV);
    axisGrid *= saturate(lineWidth / drawWidth);
    float xAxisIntensity = axisGrid.y; // z=0 line -> X-axis (red)
    float zAxisIntensity = axisGrid.x; // x=0 line -> Z-axis (blue)

    float3 gridColor = float3(0.35, 0.35, 0.35);
    float3 xAxisColor = float3(0.7, 0.2, 0.2);
    float3 zAxisColor = float3(0.2, 0.2, 0.7);

    float3 finalColor = gridColor;
    finalColor = lerp(finalColor, xAxisColor, xAxisIntensity);
    finalColor = lerp(finalColor, zAxisColor, zAxisIntensity);

    float axisPresence = max(xAxisIntensity, zAxisIntensity);
    gridAlpha *= (1.0 - axisPresence);
    float alpha = max(gridAlpha, axisPresence) * fade * gridOpacity;
    return float4(finalColor, alpha);
}
