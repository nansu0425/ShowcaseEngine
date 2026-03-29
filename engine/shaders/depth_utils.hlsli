#ifndef SHOWCASE_DEPTH_UTILS_HLSLI
#define SHOWCASE_DEPTH_UTILS_HLSLI

/// Converts a normalized linear depth [0=near, 1=far] to grayscale display color.
/// Convention: near surfaces appear BRIGHT, far surfaces appear DARK.
/// All depth preview shaders must use this function for consistent visualization.
float4 DepthToGrayscale(float normalizedDepth)
{
    float brightness = 1.0 - normalizedDepth;
    return float4(brightness, brightness, brightness, 1.0);
}

#endif
