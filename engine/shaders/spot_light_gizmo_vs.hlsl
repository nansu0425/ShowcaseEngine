cbuffer SpotLightGizmoConstants : register(b0)
{
    row_major float4x4 viewProjection; // 16 DWORDs
    float4 outerColor;                 // 4 DWORDs
    float4 innerColor;                 // 4 DWORDs
    float3 position;                   // cone apex
    float range;                       // 4 DWORDs
    float3 forward;
    float outerCosAngle; // 4 DWORDs
    float3 right;
    float innerCosAngle; // 4 DWORDs
    float3 up;
    float lineWidthPixels; // 4 DWORDs
    float2 viewportSize;
    float outerSinAngle;
    float innerSinAngle; // 4 DWORDs
}; // Total: 44 DWORDs

struct VSOutput
{
    float4 position : SV_Position;
    float4 color : COLOR0;
};

static const float PI = 3.14159265f;
static const uint SEGMENTS = 24;
static const uint VERTS_PER_QUAD = 6;
static const uint EDGE_LINES = 4;

// Per-cone vertex layout:
// Circle: SEGMENTS * VERTS_PER_QUAD = 144 vertices
// Edge lines: EDGE_LINES * VERTS_PER_QUAD = 24 vertices
// Total per cone: 168 vertices
static const uint VERTS_PER_CONE = SEGMENTS * VERTS_PER_QUAD + EDGE_LINES * VERTS_PER_QUAD;
static const uint CIRCLE_VERTS = SEGMENTS * VERTS_PER_QUAD;

// Spherical cap (outer cone only):
// Meridian arcs: 8 arcs x 5 segments = 240 vertices
static const uint CONE_TOTAL_VERTS = 2 * VERTS_PER_CONE; // 336
static const uint MERIDIAN_ARCS = 8;
static const uint MERIDIAN_SEGMENTS = 5;
static const uint MERIDIAN_VERTS = MERIDIAN_ARCS * MERIDIAN_SEGMENTS * VERTS_PER_QUAD; // 240

// Quad winding: two triangles sharing an edge
// 0--2    tri0: 0,1,2   tri1: 2,1,3
// |\ |
// | \|
// 1--3
static const uint QUAD_INDICES[6] = {0, 1, 2, 2, 1, 3};

VSOutput main(uint vertexId : SV_VertexID)
{
    VSOutput output;

    float3 w0, w1;
    float4 col;

    if (vertexId < CONE_TOTAL_VERTS)
    {
        // --- Cone geometry (outer + inner) ---
        uint coneIndex = vertexId / VERTS_PER_CONE;
        uint localId = vertexId % VERTS_PER_CONE;

        float cosAngle = (coneIndex == 0) ? outerCosAngle : innerCosAngle;
        float sinAngle = (coneIndex == 0) ? outerSinAngle : innerSinAngle;
        col = (coneIndex == 0) ? outerColor : innerColor;

        // Sphere-cone intersection: circle at distance range*cos along forward, radius range*sin
        float radius = range * sinAngle;
        float3 coneCenter = position + forward * range * cosAngle;

        if (localId < CIRCLE_VERTS)
        {
            // Circle segment
            uint segIndex = localId / VERTS_PER_QUAD;
            float a0 = 2.0f * PI * float(segIndex) / float(SEGMENTS);
            float a1 = 2.0f * PI * float(segIndex + 1) / float(SEGMENTS);

            w0 = coneCenter + right * cos(a0) * radius + up * sin(a0) * radius;
            w1 = coneCenter + right * cos(a1) * radius + up * sin(a1) * radius;
        }
        else
        {
            // Edge line from apex to circle perimeter
            uint edgeLocalId = localId - CIRCLE_VERTS;
            uint edgeIndex = edgeLocalId / VERTS_PER_QUAD;
            float angle = 2.0f * PI * float(edgeIndex) / float(EDGE_LINES);

            w0 = position;
            w1 = coneCenter + right * cos(angle) * radius + up * sin(angle) * radius;
        }
    }
    else
    {
        // --- Spherical cap (outer cone only) ---
        uint capId = vertexId - CONE_TOTAL_VERTS;
        col = outerColor;
        float outerAngle = acos(outerCosAngle);

        // Meridian arc: from pole (forward * range) to outer circle along sphere surface
        uint arcIndex = capId / (MERIDIAN_SEGMENTS * VERTS_PER_QUAD);
        uint segInArc = (capId % (MERIDIAN_SEGMENTS * VERTS_PER_QUAD)) / VERTS_PER_QUAD;

        float phi = 2.0f * PI * float(arcIndex) / float(MERIDIAN_ARCS);
        float3 radialDir = right * cos(phi) + up * sin(phi);

        float theta0 = outerAngle * float(segInArc) / float(MERIDIAN_SEGMENTS);
        float theta1 = outerAngle * float(segInArc + 1) / float(MERIDIAN_SEGMENTS);

        w0 = position + (forward * cos(theta0) + radialDir * sin(theta0)) * range;
        w1 = position + (forward * cos(theta1) + radialDir * sin(theta1)) * range;
    }

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
    uint vertInQuad = vertexId % VERTS_PER_QUAD;
    uint qi = QUAD_INDICES[vertInQuad];
    float4 clipPos = (qi < 2) ? clip0 : clip1;
    float side = (qi % 2 == 0) ? -1.0f : 1.0f;

    // Apply offset in clip space
    clipPos.xy += ndcOffset * side * clipPos.w;

    output.position = clipPos;
    output.color = col;
    return output;
}
