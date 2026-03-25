# Lighting Roadmap

Phased lighting implementation plan for ShowcaseEngine. Each phase produces a visible improvement suitable for YouTube content targeting non-developers. The **Cornell Box** scene serves as the standard demo scene throughout all phases — its enclosed room with colored walls makes every lighting change dramatically visible.

## Demo Scene: Cornell Box

A 5m × 5m × 5m enclosed room:
- **Left wall**: Red — **Right wall**: Green — **Others**: White
- Interior objects: Tall box (rotated cube) + Sphere
- Camera viewing from the open front

This scene was designed in 1984 at Cornell University specifically to test lighting accuracy. Colored walls reveal color bleeding (GI), flat surfaces show shadow reception, and the sphere's curved surface highlights shading quality at every phase.

---

## Phase 0: Scene Setup

> **Viewer impact**: Baseline — flat colors, no sense of depth or lighting

### Goal
Build the Cornell Box scene from engine primitives before any lighting code exists. This becomes the "before" reference for all subsequent phases.

### Technical scope
- Add `builtin:plane` and `builtin:sphere` primitives to engine
- Add per-object `baseColorOverride` to assign different colors to shared primitives
- Create `cornell_box.scene` file with walls, box, and sphere

### Prerequisites
None (starting point)

### Key systems affected
- SceneRenderer (geometry generation)
- AssetManager (primitive registration)
- Scene (color override, serialization)

### Visual impact: N/A (foundation)

---

## Phase 1: Diffuse Light (Lambertian)

> **Viewer impact**: "The flat world suddenly becomes 3D — surfaces facing the light are bright, others are dark"

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

> **Viewer impact**: "The sphere now has a shiny bright spot that moves as I move the camera"

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

## Phase 3: Multiple Lights (Point Light)

> **Viewer impact**: "A light bulb inside the room — things near it glow, things far away stay dark"

### Goal
Support multiple light sources with distance-based attenuation.

### Technical scope
- Define light data structure (type, position, direction, color, intensity, range)
- Distance-based attenuation for point lights
- Scene-level light storage and serialization
- Editor UI: add/position lights, visualize light gizmos

### Content point
"Light in real life gets weaker with distance — games simulate this with a mathematical falloff curve. Moving the light around changes everything in real time"

### Prerequisites
Phase 2 (specular model to show point light reflections)

### Key systems affected
- Shaders (light loop in pixel shader)
- Scene (light data, serialization)
- Editor (light placement UI)

### Visual impact: ★★★★☆

---

## Phase 4: Shadow Mapping

> **Viewer impact**: "Shadows appear behind the box and sphere!"

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

> **Viewer impact**: "The wall looks bumpy even though it's completely flat geometry!"

### Goal
Add tangent-space normal mapping to simulate surface detail without additional geometry.

### Technical scope
- Extend vertex format with tangent vectors (float4, w = handedness)
- Compute TBN matrix (tangent, bitangent, normal) in vertex shader
- Sample normal map texture, transform from tangent space to world space
- Update glTF loader to extract tangent data

### Demo scene note
Cornell Box's clean flat surfaces don't showcase normal mapping well. A separate demo scene with textured surfaces (e.g. stone, wood, brick) will be needed — to be designed at implementation time.

### Content point
"Games' most powerful illusion — a flat surface that looks bumpy. The normal map tells each pixel to pretend it's facing a slightly different direction, and the lighting math does the rest"

### Prerequisites
Phase 1 (diffuse lighting reacts to normals)

### Key systems affected
- Vertex format (ModelVertex)
- Shaders (TBN computation)
- Model loader (tangent extraction)
- Material system (normal map texture slot)

### Visual impact: ★★★★☆

---

## Phase 6: PBR (Physically Based Rendering)

> **Viewer impact**: "Metal looks like metal, plastic looks like plastic — same lighting, completely different look"

### Goal
Replace Blinn-Phong with a physically based BRDF using the metallic/roughness workflow.

### Technical scope
- Physically based BRDF replacing Blinn-Phong specular
- Metallic/roughness material parameters (already in glTF PBR spec)
- Energy conservation between diffuse and specular
- Update glTF loader to extract metallic/roughness values and textures

### Demo scene note
Cornell Box lacks material variety. A separate demo scene comparing multiple materials (metal, plastic, rough, smooth) will be needed — to be designed at implementation time.

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

> **Viewer impact**: "Corners and crevices are naturally darker — subtle but everything feels more grounded"

### Goal
Add Screen-Space Ambient Occlusion to darken areas where ambient light is occluded.

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

## Phase 8: Global Illumination

> **Viewer impact**: "The white object near the red wall turns slightly red — light bounces between surfaces!"

### Goal
Add indirect lighting so light bounces between surfaces, carrying color. This is the Cornell Box's signature effect.

### Technical scope
- At minimum: one-bounce indirect diffuse
- Cornell Box red/green color bleeding as primary validation target

### Content point
"The holy grail of real-time graphics — light that bounces. When light hits the red wall and reflects onto nearby surfaces, everything gets a red tint. This is what separates 'good' lighting from 'photorealistic'"

### Prerequisites
All previous phases (full lighting pipeline)

### Key systems affected
- Major rendering pipeline changes (technique-dependent)
- Potentially new data structures (probes, voxels)

### Visual impact: ★★★★★

---

## Summary

| Phase | Feature | Visual Impact | Content Hook |
|-------|---------|:---:|---|
| 0 | Scene Setup | — | Baseline "before" |
| 1 | Diffuse Light | ★★★★★ | "Flat to 3D with one dot product" |
| 2 | Specular Highlight | ★★★☆☆ | "Your viewpoint changes everything" |
| 3 | Multiple Lights | ★★★★☆ | "Distance makes light fade" |
| 4 | Shadow Mapping | ★★★★★ | "Light has its own camera" |
| 5 | Normal Mapping | ★★★★☆ | "Games' greatest illusion" |
| 6 | PBR | ★★★★☆ | "Physics makes materials real" |
| 7 | Ambient Occlusion | ★★★☆☆ | "Why corners are dark" |
| 8 | Global Illumination | ★★★★★ | "Light that bounces" |

---

## Beyond This Roadmap

**Ray Tracing** is a fundamentally different rendering paradigm (tracing light rays instead of rasterizing triangles) that can solve shadows, reflections, and GI more accurately. It will be covered as a separate series — the rasterization techniques above provide the conceptual foundation that makes ray tracing's advantages intuitive to understand.
