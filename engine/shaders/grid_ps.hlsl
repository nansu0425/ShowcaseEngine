cbuffer PerFrame : register(b0) {
    row_major float4x4 viewProjection;
    float3 cameraPosition;
    float gridOpacity;
    float nearPlane;
    float farPlane;
};

Texture2D<float> sceneDepth : register(t1);

struct PSInput {
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float3 worldPos : TEXCOORD0;
};

// Pristine Grid: anti-aliased grid pattern at any scale.
// coord:    world position divided by grid spacing
// uvDeriv:  screen-space derivatives at that scale
// lineWidth: per-axis line thickness as fraction of cell (0-0.5)
float PristineGrid(float2 coord, float2 uvDeriv, float2 lineWidth, float2 fadeWidth) {
    float2 drawWidth = clamp(lineWidth, uvDeriv, 0.5);
    float2 lineAA = uvDeriv * 1.5;
    float2 gridUV = 1.0 - abs(frac(coord) * 2.0 - 1.0);
    float2 grid2 = smoothstep(drawWidth + lineAA, drawWidth - lineAA, gridUV);
    grid2 *= saturate(fadeWidth / drawWidth);
    grid2 = lerp(grid2, lineWidth, saturate(uvDeriv * 2.0 - 1.0));
    return lerp(grid2.x, 1.0, grid2.y);
}

// Returns visibility weight [0,1] based on screen-space density.
// When grid cells are large on screen (low density) -> 1.0
// When grid cells approach sub-pixel (high density) -> 0.0
float LODFade(float2 scaledDeriv) {
    float density = max(scaledDeriv.x, scaledDeriv.y);
    return 1.0 - smoothstep(0.15, 0.5, density);
}

// Axis line: same as PristineGrid but for the origin line (abs(coord) instead of frac-based).
// Returns per-component intensity for x=0 and z=0 lines.
float2 AxisLine(float2 coord, float2 uvDeriv, float2 lineWidth, float2 fadeWidth) {
    float2 axisUV = abs(coord);
    float2 drawWidth = clamp(lineWidth, uvDeriv, 0.5);
    float2 lineAA = uvDeriv * 1.5;
    float2 grid2 = smoothstep(drawWidth + lineAA, drawWidth - lineAA, axisUV);
    grid2 *= saturate(fadeWidth / drawWidth);
    grid2 = lerp(grid2, lineWidth, saturate(uvDeriv * 2.0 - 1.0));
    return grid2;
}

float4 main(PSInput input) : SV_TARGET {
    float2 coord = input.worldPos.xz;
    float pixelWidth = 1.0; // grid line thickness in pixels (screen-space constant)

    // Screen-space derivatives (shared across all LOD levels)
    float2 uvDDX = ddx(coord);
    float2 uvDDY = ddy(coord);
    float2 uvDeriv = float2(length(float2(uvDDX.x, uvDDY.x)), length(float2(uvDDX.y, uvDDY.y)));

    // --- Multi-LOD grid: 1m / 10m / 100m ---
    // Opacity hierarchy: coarser grids are brighter for visual scale distinction
    static const float3 opacities = float3(0.6, 0.8, 1.0);

    // LOD 0: 1m grid — per-axis lineWidth for correct anisotropic scaling
    float2 deriv0 = uvDeriv;
    float2 lineWidth0 = min(deriv0 * pixelWidth, 0.15);
    float alpha0 = PristineGrid(coord, deriv0, lineWidth0, lineWidth0) * opacities.x * LODFade(deriv0);

    // LOD 1: 10m grid
    float2 deriv1 = uvDeriv / 10.0;
    float2 lineWidth1 = min(deriv1 * pixelWidth, 0.15);
    float alpha1 = PristineGrid(coord / 10.0, deriv1, lineWidth1, lineWidth1) * opacities.y * LODFade(deriv1);

    // LOD 2: 100m grid — no LODFade (coarsest level, nothing to fall back to)
    // Pristine Grid's own Nyquist fade handles sub-pixel gracefully
    float2 deriv2 = uvDeriv / 100.0;
    float2 lineWidth2 = min(deriv2 * pixelWidth, 0.15);
    float alpha2 = PristineGrid(coord / 100.0, deriv2, lineWidth2, lineWidth2) * opacities.z;

    // Composite: max() prevents double-darkening where lines overlap
    float gridAlpha = max(max(alpha0, alpha1), alpha2);

    // --- Axis coloring (x=0 red, z=0 blue) — matches grid width at every LOD ---
    float2 axis0 = AxisLine(coord, deriv0, lineWidth0, lineWidth0) * opacities.x * LODFade(deriv0);
    float2 axis1 = AxisLine(coord / 10.0, deriv1, lineWidth1, lineWidth1) * opacities.y * LODFade(deriv1);
    float2 axis2 = AxisLine(coord / 100.0, deriv2, lineWidth2, lineWidth2) * opacities.z;

    float xAxisIntensity = max(max(axis0.y, axis1.y), axis2.y); // z=0 line -> X-axis (red)
    float zAxisIntensity = max(max(axis0.x, axis1.x), axis2.x); // x=0 line -> Z-axis (blue)

    float3 gridColor = float3(0.35, 0.35, 0.35);
    float3 xAxisColor = float3(0.7, 0.2, 0.2);
    float3 zAxisColor = float3(0.2, 0.2, 0.7);

    float3 finalColor = gridColor;
    finalColor = lerp(finalColor, xAxisColor, xAxisIntensity);
    finalColor = lerp(finalColor, zAxisColor, zAxisIntensity);

    float axisPresence = max(xAxisIntensity, zAxisIntensity);
    gridAlpha *= (1.0 - axisPresence);
    float alpha = max(gridAlpha, axisPresence) * gridOpacity;

    // Manual depth comparison in linear space: distance-invariant occlusion.
    // NDC depth is nonlinear (1/z) — a fixed epsilon fails at distance.
    // Converting to linear view-space depth lets us use a world-space tolerance (1cm).
    float depth = sceneDepth.Load(int3(input.position.xy, 0));
    // Recompute grid depth analytically from world position.
    // input.position.z suffers from GPU near-plane clipping noise on the 3000m quad;
    // reproject from worldPos (precise TEXCOORD interpolation) instead.
    float4 gridClip = mul(float4(input.worldPos.x, 0.0, input.worldPos.z, 1.0), viewProjection);
    float gridDepth = gridClip.z / gridClip.w;
    float linearScene = nearPlane * farPlane / (farPlane - depth * (farPlane - nearPlane));
    float linearGrid = nearPlane * farPlane / (farPlane - gridDepth * (farPlane - nearPlane));
    // Adaptive epsilon: NDC interpolation noise scales as z² in linear space.
    // noiseEpsilon tracks this growth; 0.005m floor for close-range precision.
    float noiseEpsilon = 0.0002 * linearGrid * linearGrid / (nearPlane * farPlane);
    float epsilon = max(noiseEpsilon, 0.005);
    if (linearScene <= linearGrid + epsilon) {
        discard;
    }

    clip(alpha - 0.01);
    return float4(finalColor, alpha);
}
