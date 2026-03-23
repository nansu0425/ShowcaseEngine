cbuffer PerFrame : register(b0) {
    row_major float4x4 viewProjection;
    float3 cameraPosition;
    float gridOpacity;
};

struct PSInput {
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float3 worldPos : TEXCOORD0;
};

// Pristine Grid: anti-aliased grid pattern at any scale.
// coord:    world position divided by grid spacing
// uvDeriv:  screen-space derivatives at that scale
// lineWidth: line thickness as fraction of cell (0-0.5)
float PristineGrid(float2 coord, float2 uvDeriv, float lineWidth, float fadeWidth) {
    float2 drawWidth = clamp(float2(lineWidth, lineWidth), uvDeriv, 0.5);
    float2 lineAA = uvDeriv * 1.5;
    float2 gridUV = 1.0 - abs(frac(coord) * 2.0 - 1.0);
    float2 grid2 = smoothstep(drawWidth + lineAA, drawWidth - lineAA, gridUV);
    grid2 *= saturate(fadeWidth / drawWidth);
    grid2 = lerp(grid2, float2(lineWidth, lineWidth), saturate(uvDeriv * 2.0 - 1.0));
    return lerp(grid2.x, 1.0, grid2.y);
}

// Returns visibility weight [0,1] based on screen-space density.
// When grid cells are large on screen (low density) -> 1.0
// When grid cells approach sub-pixel (high density) -> 0.0
float LODFade(float2 scaledDeriv) {
    float density = max(scaledDeriv.x, scaledDeriv.y);
    return 1.0 - smoothstep(0.15, 0.5, density);
}

float4 main(PSInput input) : SV_TARGET {
    float2 coord = input.worldPos.xz;
    float lineWidth0 = 0.04;   // 1m grid:   4cm
    float lineWidth1 = 0.008;  // 10m grid:  8cm
    float lineWidth2 = 0.0016; // 100m grid: 16cm
    float fadeWidth = 0.04;    // shared fade distance (decoupled from visual thickness)

    // Screen-space derivatives (shared across all LOD levels)
    float2 uvDDX = ddx(coord);
    float2 uvDDY = ddy(coord);
    float2 uvDeriv = float2(length(float2(uvDDX.x, uvDDY.x)), length(float2(uvDDX.y, uvDDY.y)));

    // --- Multi-LOD grid: 1m / 10m / 100m ---
    // Opacity hierarchy: coarser grids are brighter for visual scale distinction
    static const float3 opacities = float3(0.6, 0.8, 1.0);

    // LOD 0: 1m grid
    float2 deriv0 = uvDeriv;
    float alpha0 = PristineGrid(coord, deriv0, lineWidth0, fadeWidth) * opacities.x * LODFade(deriv0);

    // LOD 1: 10m grid
    float2 deriv1 = uvDeriv / 10.0;
    float alpha1 = PristineGrid(coord / 10.0, deriv1, lineWidth1, fadeWidth) * opacities.y * LODFade(deriv1);

    // LOD 2: 100m grid — no LODFade (coarsest level, nothing to fall back to)
    // Pristine Grid's own Nyquist fade handles sub-pixel gracefully
    float2 deriv2 = uvDeriv / 100.0;
    float alpha2 = PristineGrid(coord / 100.0, deriv2, lineWidth2, fadeWidth) * opacities.z;

    // Composite: max() prevents double-darkening where lines overlap
    float gridAlpha = max(max(alpha0, alpha1), alpha2);

    // --- Axis coloring (x=0 red, z=0 blue) — visible at all distances ---
    float2 axisUV = abs(coord);
    float2 drawWidth = clamp(float2(lineWidth0, lineWidth0), uvDeriv, 0.5);
    float2 lineAA = uvDeriv * 1.5;
    float2 axisGrid = smoothstep(drawWidth + lineAA, drawWidth - lineAA, axisUV);
    axisGrid *= saturate(lineWidth0 / drawWidth);
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
    float alpha = max(gridAlpha, axisPresence) * gridOpacity;

    return float4(finalColor, alpha);
}
