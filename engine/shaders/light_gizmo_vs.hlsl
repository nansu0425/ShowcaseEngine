cbuffer LightGizmoConstants : register(b0)
{
    row_major float4x4 viewProjection; // 16 DWORDs
    float4 color;                      // 4 DWORDs
    float4 centerAndRange;             // 4 DWORDs (.xyz = center, .w = range)
    float2 viewportSize;               // 2 DWORDs (width, height in pixels)
    float lineWidthPixels;             // 1 DWORD
    float _pad;                        // 1 DWORD
}; // Total: 28 DWORDs

struct VSOutput
{
    float4 position : SV_Position;
    float4 color : COLOR0;
};

static const float PI = 3.14159265f;
static const uint SEGMENTS = 24;
static const uint VERTS_PER_QUAD = 6; // 2 triangles per line segment
static const uint QUADS_PER_RING = SEGMENTS;

// Quad winding: two triangles sharing an edge
// 0--2    tri0: 0,1,2   tri1: 2,1,3
// |\ |    vertex 0: p0 - offset
// | \|    vertex 1: p0 + offset
// 1--3    vertex 2: p1 - offset
//         vertex 3: p1 + offset
static const uint QUAD_INDICES[6] = {0, 1, 2, 2, 1, 3};

float3 RingPoint(uint ringIndex, float angle)
{
    float c = cos(angle);
    float s = sin(angle);
    if (ringIndex == 0)
        return float3(0, c, s); // YZ plane
    if (ringIndex == 1)
        return float3(c, 0, s); // XZ plane
    return float3(c, s, 0);     // XY plane
}

VSOutput main(uint vertexId : SV_VertexID)
{
    VSOutput output;

    uint segGlobal = vertexId / VERTS_PER_QUAD;  // 0..71 (3 rings x 24 segments)
    uint vertInQuad = vertexId % VERTS_PER_QUAD; // 0..5

    uint ringIndex = segGlobal / QUADS_PER_RING; // 0, 1, 2
    uint segIndex = segGlobal % QUADS_PER_RING;  // 0..23

    // Segment endpoints in world space
    float a0 = 2.0f * PI * float(segIndex) / float(SEGMENTS);
    float a1 = 2.0f * PI * float(segIndex + 1) / float(SEGMENTS);

    float3 w0 = centerAndRange.xyz + RingPoint(ringIndex, a0) * centerAndRange.w;
    float3 w1 = centerAndRange.xyz + RingPoint(ringIndex, a1) * centerAndRange.w;

    // Transform to clip space
    float4 clip0 = mul(float4(w0, 1.0f), viewProjection);
    float4 clip1 = mul(float4(w1, 1.0f), viewProjection);

    // Screen-space direction for perpendicular offset
    float2 ndc0 = clip0.xy / clip0.w;
    float2 ndc1 = clip1.xy / clip1.w;
    float2 diff = (ndc1 - ndc0) * viewportSize;
    float len = length(diff);
    float2 dir = len > 1e-5f ? diff / len : float2(1.0f, 0.0f);
    float2 perp = float2(-dir.y, dir.x);

    // Perpendicular offset in NDC (half-width each side)
    float2 ndcOffset = perp * lineWidthPixels / viewportSize;

    // Select endpoint and side from quad index
    uint qi = QUAD_INDICES[vertInQuad];
    float4 clipPos = (qi < 2) ? clip0 : clip1;
    float side = (qi % 2 == 0) ? -1.0f : 1.0f;

    // Apply offset in clip space
    clipPos.xy += ndcOffset * side * clipPos.w;

    output.position = clipPos;
    output.color = color;
    return output;
}
