# Lighting Roadmap

Phased lighting implementation plan for ShowcaseEngine. Each phase produces a visible improvement suitable for YouTube content targeting non-developers. The **Sponza Atrium** scene serves as the standard demo scene throughout all phases — its complex architecture with columns, arches, and varied materials makes every lighting change dramatically visible.

## Demo Scene: Sponza Atrium

A detailed architectural scene of a Venetian palace courtyard (glTF model with PBR data):
- **Complex geometry**: Columns, arches, balconies, and vegetation provide natural shadow casters and receivers
- **25 PBR materials**: Stone, metal, fabric, and foliage — each responds differently to lighting
- **Texture data**: 24 normal maps and 24 metallic-roughness maps embedded in the glTF, ready to unlock as the pipeline matures

Sponza is an industry-standard benchmark scene for real-time rendering. Its architectural complexity reveals shading quality at every phase — columns show diffuse gradients, stone surfaces highlight specular and normal mapping, arches cast dramatic shadows, and tight corners expose ambient occlusion.

---

## Phase 0: Scene Setup

> **Viewer impact**: Baseline — textured but flat, no sense of depth or lighting

### Goal
Load the Sponza Atrium scene before any lighting code exists. This becomes the "before" reference for all subsequent phases.

### Technical scope
- Load Sponza glTF model with all base color textures rendering correctly
- Ensure alpha-masked materials (vegetation) display properly
- Create `sponza.scene` file with camera positioned inside the atrium

### Prerequisites
None (starting point)

### Key systems affected
- SceneRenderer (glTF rendering)
- Model loader (base color texture extraction)
- Scene (camera setup, serialization)

### Visual impact: N/A (foundation)

---

## Phase 1: Diffuse Light (Lambertian)

> **Viewer impact**: "The flat atrium suddenly becomes 3D — columns cast bright faces toward the light while their backs go dark"

### Goal
Add a single directional light with Lambertian diffuse shading. This is the single biggest visual leap in the entire roadmap.

### Technical scope
- Expand `PerFrameData` constant buffer with light direction, color, intensity, ambient
- Vertex shader: transform normals to world space and pass to pixel shader
- Pixel shader: `N·L` diffuse calculation + ambient term
- Hardcoded directional light (no scene serialization yet)

### Content point
"The most basic trick that makes game objects look 3D — a single dot product between the surface direction and the light direction"

### Prerequisites
Phase 0 (scene exists with normals in vertex data)

### Key systems affected
- Shaders (mesh_vs.hlsl, mesh_ps.hlsl)
- SceneRenderer (constant buffer layout)

### Visual impact: ★★★★★

---

## Phase 2: Specular Highlight (Blinn-Phong)

> **Viewer impact**: "Stone columns catch bright highlights, metal railings gleam as I move the camera"

### Goal
Add view-dependent specular highlights using the Blinn-Phong model.

### Technical scope
- Pixel shader: compute half-vector `H = normalize(V + L)`, specular = `pow(N·H, shininess)`
- Per-material shininess/specular color (extend material constant buffer)
- Camera position already available in `PerFrameData`

### Content point
"Why shiny objects change as you move — your viewing angle matters. The 'specular highlight' is a reflection of the light source itself"

### Prerequisites
Phase 1 (diffuse lighting, world normals in pixel shader)

### Key systems affected
- Shaders (mesh_ps.hlsl)
- SceneRenderer (material CB extension)

### Visual impact: ★★★☆☆

---

## Phase 3: Multiple Lights (Point & Spot)

> **Viewer impact**: "Warm point lights glow under the arches, a spotlight picks out a single column — the atrium comes alive"

### Goal
Support multiple light sources with distance-based and cone-based attenuation.

### Technical scope
- Define light data structure (type, position, direction, color, intensity, range)
- Point light: distance-based attenuation (light bulb — radiates in all directions, fades with distance)
- Spot light: inner/outer cone angle with directional falloff (flashlight — aimed beam that fades at the edges)
- Scene-level light storage and serialization
- Editor UI: add/position lights, visualize light gizmos

### Content point
"Three kinds of light in games — the sun (directional), a light bulb (point), and a flashlight (spot). Each fades differently, and placing them in a scene is how game developers set the mood"

### Prerequisites
Phase 2 (specular model to show point light reflections)

### Key systems affected
- Shaders (light loop in pixel shader)
- Scene (light data, serialization)
- Editor (light placement UI)

### Visual impact: ★★★★☆

---

## Phase 4: Shadow Mapping

> **Viewer impact**: "Columns cast long shadows across the atrium floor, arches project shadow patterns onto the walls!"

### Goal
Render shadows from the directional light using a depth map.

### Technical scope
- Shadow pass: render scene depth from light's perspective into a shadow map texture
- Shadow sampling: project each pixel into light space, compare depth to shadow map
- Start with hard shadows, then add PCF (Percentage Closer Filtering) for soft edges

### Content point
"Shadows are made by rendering the scene from the light's point of view — if the light can't see a surface, that surface is in shadow. The light has its own camera"

### Prerequisites
Phase 1 (directional light exists)

### Key systems affected
- New shadow pass shaders
- SceneRenderer (multi-pass rendering, shadow map texture)
- Root signature (shadow map SRV)

### Visual impact: ★★★★★

---

## Phase 5: Normal Mapping

> **Viewer impact**: "The stone walls look rough and weathered even though the geometry is flat!"

### Goal
Add tangent-space normal mapping to simulate surface detail without additional geometry. Sponza's glTF already contains 24 normal map textures — this phase unlocks them.

### Technical scope
- Extend vertex format with tangent vectors (float4, w = handedness)
- Compute TBN matrix (tangent, bitangent, normal) in vertex shader
- Sample normal map texture, transform from tangent space to world space
- Update glTF loader to extract tangent data and normal map textures (data exists in file but is currently ignored)

### Content point
"Games' most powerful illusion — a flat surface that looks bumpy. The normal map tells each pixel to pretend it's facing a slightly different direction, and the lighting math does the rest"

### Prerequisites
Phase 1 (diffuse lighting reacts to normals)

### Key systems affected
- Vertex format (ModelVertex)
- Shaders (TBN computation)
- Model loader (tangent and normal map extraction)
- Material system (normal map texture slot)

### Visual impact: ★★★★☆

---

## Phase 6: PBR (Physically Based Rendering)

> **Viewer impact**: "The same Sponza scene, but now stone looks rough, metal railings gleam, and fabric absorbs light — each material feels real"

### Goal
Replace Blinn-Phong with a physically based BRDF using the metallic/roughness workflow. Sponza's glTF already contains 25 PBR materials with metallic/roughness textures — this phase unlocks them.

### Technical scope
- Physically based BRDF replacing Blinn-Phong specular
- Metallic/roughness material parameters (already in glTF PBR spec)
- Energy conservation between diffuse and specular
- Update glTF loader to extract metallic/roughness values and textures (data exists in file but is currently ignored)

### Content point
"Modern games use real physics equations for light — the same math explains why metal reflects differently from rubber. Two numbers (metallic, roughness) control everything"

### Prerequisites
Phase 2 (specular model to replace), Phase 5 (normal maps enhance PBR)

### Key systems affected
- Shaders (complete BRDF rewrite in pixel shader)
- Material system (metallic, roughness, AO textures)
- Model loader (PBR texture extraction)

### Visual impact: ★★★★☆

---

## Phase 7: Ambient Occlusion (SSAO)

> **Viewer impact**: "Where columns meet the floor, where arches curve into walls — every crevice darkens naturally"

### Goal
Add Screen-Space Ambient Occlusion to darken areas where ambient light is occluded. Sponza's dense architectural geometry — column bases, arch interiors, balcony undersides — makes SSAO differences immediately visible.

### Technical scope
- Screen-space technique to darken occluded areas (corners, crevices, contact points)
- Composite AO factor into final lighting

### Content point
"In reality, corners are darker because light bounces have trouble reaching them. SSAO fakes this by checking how 'enclosed' each pixel is — cheap but effective"

### Prerequisites
Phase 1 (ambient lighting term to modulate)

### Key systems affected
- New SSAO pass (compute or pixel shader)
- Render pipeline (additional render targets)
- Post-processing infrastructure

### Visual impact: ★★★☆☆

---

## Summary

| Phase | Feature | Visual Impact | Content Hook |
|-------|---------|:---:|---|
| 0 | Scene Setup | — | Baseline "before" |
| 1 | Diffuse Light | ★★★★★ | "Flat to 3D with one dot product" |
| 2 | Specular Highlight | ★★★☆☆ | "Your viewpoint changes everything" |
| 3 | Multiple Lights (Point & Spot) | ★★★★☆ | "Sun, bulb, and flashlight" |
| 4 | Shadow Mapping | ★★★★★ | "Columns and arches cast shadows" |
| 5 | Normal Mapping | ★★★★☆ | "Games' greatest illusion" |
| 6 | PBR | ★★★★☆ | "Physics makes materials real" |
| 7 | Ambient Occlusion | ★★★☆☆ | "Why corners are dark" |

---

## YouTube Episodes

Each episode groups related phases into a single video with a clear theme for non-developer viewers.

| Episode | Phases | Theme |
|---------|--------|-------|
| 1 | 0 → 1 → 2 | "빛이 세상을 바꾼다" |
| 2 | 3 | "다중 광원" |
| 3 | 4 | "그림자의 원리" |
| 4 | 5 → 6 | "재질의 비밀" |
| 5 | 7 | "구석이 어두운 이유" |

- **Ep 1** — Unlit → diffuse → specular: a single light transforms the flat Sponza atrium into a 3D space
- **Ep 2** — Point lights and spot lights: place lights interactively in the editor, show distance attenuation and cone falloff
- **Ep 3** — Shadow mapping: columns and arches cast dramatic shadows across the atrium floor
- **Ep 4** — Normal mapping + PBR: unlock the texture data already embedded in Sponza's glTF — stone becomes rough, metal gleams
- **Ep 5** — SSAO: where columns meet the floor, where arches curve into walls — every crevice darkens naturally

---

## Beyond This Roadmap

**Ray Tracing** is a fundamentally different rendering paradigm (tracing light rays instead of rasterizing triangles) that can solve shadows, reflections, and GI more accurately. **Global Illumination** — where light bounces between surfaces carrying color — is best demonstrated with ray tracing rather than rasterization approximations. Both will be covered as a separate series — the rasterization techniques above provide the conceptual foundation that makes ray tracing's advantages intuitive to understand.
