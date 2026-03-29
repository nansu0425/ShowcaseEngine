# Spot Light Shadow Debugging Visualization Tools

## Context

The engine has 4 directional light debug tools (Shadow Info, Shadow Frustum Wireframe, Shadow Map Preview, Shadow Coverage Overlay) and 6 point light debug tools (Debug Info, Shadow Coverage Overlay, Cubemap Face ID, Cubemap Face Preview, Depth Precision Heatmap, Range Gizmo), all fully implemented. Spot light shadows use 2D perspective depth buffers (1024x1024, max 8 shadow-casting lights, 3x3 PCF, FOV = 2 × outerAngle), but have only two debug visualizations: a dual-cone wireframe gizmo and basic shadow stats.

Spot light shadows introduce unique debugging challenges compared to directional and point:
- **Dual attenuation** — distance falloff × angular falloff interact to produce non-intuitive intensity patterns
- **Perspective frustum** — FOV directly coupled to outerAngle parameter, shadow coverage depends on cone geometry
- **Inner/outer cone boundary** — falloff region between inner and outer cone is invisible without visualization
- **Shadow-cone mismatch** — shadow frustum and lighting cone share the same outerAngle, but shadow artifacts can appear near cone edges

## Proposed Tools

### Tool 1: Spot Shadow Coverage Overlay
**Complexity: MEDIUM | Priority: 1st**

Fullscreen post-process overlay for a selected spot light. Same visual language as directional and point shadow coverage overlays. Color-codes surfaces by shadow state: **Red** = shadowed, **Green** = lit, **Blue tint** = outside cone or range (no contribution). Blue for "outside cone" is especially valuable since the conical boundary is harder to mentally trace than a spherical (point) or infinite (directional) boundary.

Implementation:
- New shader `spot_shadow_overlay_ps.hlsl`:
  - Read scene depth buffer, reconstruct world position via `invViewProjection`
  - Compute distance from world position to light position — if distance > range: output blue
  - Compute `cosAngle = dot(direction, normalize(lightToFrag))` — if cosAngle < outerCos: output blue (outside cone)
  - Otherwise: sample spot shadow map (reuse `CalculateSpotShadow` logic from `mesh_ps.hlsl`), lerp red↔green
- New PSO + root signature:
  - Root constants (~28 DWORDs): `invViewProjection` (16), `spotShadowVP` (16) — split across two root constant ranges or use a single 44-DWORD block with: light position (3), range (1), direction (3), outerCos (1), shadow bias (1), shadow map texel size (1), overlay alpha (1), padding (1)
  - SRV: scene depth buffer (t0), spot shadow map (t1)
  - Samplers: point-clamp (s0), comparison (s1)
  - Alpha blend, no depth write
- Add `RenderSpotShadowOverlay()` to `SceneRenderer`
- `m_showSpotShadowOverlay` flag in `ViewportPanel`
- Console command `toggle_spot_shadow_overlay`
- Checkbox in inspector under spot light debug section
- Config persistence in `EditorApp`

New files:
- `engine/shaders/spot_shadow_overlay_ps.hlsl`

Files to modify:
- `engine/include/showcase/graphics/SceneRenderer.h` — PSO, root sig, render method
- `engine/src/graphics/SceneRenderer.cpp` — PSO init, render implementation
- `engine/CMakeLists.txt` — register new shader
- `editor/include/showcase/editor/ViewportPanel.h` — toggle flag
- `editor/src/EditorApp.cpp` — render loop, console command, config
- `editor/src/EditorController.cpp` — checkbox in inspector

---

### Tool 2: Spot Shadow Frustum Visualization
**Complexity: LOW | Priority: 2nd**

Wireframe visualization of the spot light's perspective shadow frustum — a truncated pyramid shape. Unlike the directional light's orthographic box, this frustum is tapered, visually demonstrating the perspective projection. The frustum shape directly maps to `FOV = 2 × outerAngle`, so adjusting outerAngle visibly changes the shadow coverage volume.

Implementation:
- **Reuses existing shaders**: `frustum_debug_vs.hlsl` + `frustum_debug_ps.hlsl` — these accept 8 arbitrary world-space corners, so they work for any frustum shape (orthographic box or perspective pyramid)
- **Reuses existing PSOs**: `m_frustumDebugTriPSO` (semi-transparent fill) + `m_frustumDebugLinePSO` (wireframe edges)
- Compute 8 frustum corners by inverting `m_spotShadowVPs[idx]`: transform NDC corners (±1, ±1, 0/1) to world space via `inverse(spotShadowVP)`
- Use distinct color to differentiate from directional frustum: yellow `{1.0, 0.9, 0.2}` (directional uses orange `{1.0, 0.7, 0.0}`)
- Add `SpotShadowFrustumDebugData` struct with 8 corners + valid flag, populated during `RenderSpotShadowPass()`
- Add `RenderSpotShadowFrustum()` to `SceneRenderer`

New files:
- None (reuses existing shaders and PSOs)

Files to modify:
- `engine/include/showcase/graphics/SceneRenderer.h` — frustum debug data struct, render method
- `engine/src/graphics/SceneRenderer.cpp` — corner computation in shadow pass, render implementation
- `editor/include/showcase/editor/ViewportPanel.h` — toggle flag
- `editor/src/EditorApp.cpp` — render loop, console command, config
- `editor/src/EditorController.cpp` — checkbox in inspector

---

### Tool 3: Spot Shadow Map Preview
**Complexity: MEDIUM | Priority: 3rd**

Grayscale depth image of the spot light's shadow map displayed in the inspector panel. Shows what the light "sees" from its perspective projection. Unlike the directional shadow preview (orthographic, uniform depth distribution), perspective depth is non-linear — close objects appear much lighter, distant objects darker. The circular cone boundary is also visible where the cone intersects geometry.

Implementation:
- New shader `spot_shadow_preview_ps.hlsl`:
  - Input: `Texture2D<float>` spot shadow depth map (t0)
  - Root constants: `nearZ` (float), `farZ` (float)
  - Linearize perspective depth: `linearDepth = near * far / (far - storedDepth * (far - near))`
  - Normalize: `normalized = linearDepth / far`
  - Output: `float4(normalized, normalized, normalized, 1.0)`
- New PSO + root signature:
  - Root constants (4 DWORDs): nearZ, farZ, padding ×2
  - SRV: spot shadow depth map (t0)
  - Point-clamp sampler (s0)
- Preview render target: `m_spotShadowPreviewRT` (256×256 or 512×512, `DXGI_FORMAT_R8G8B8A8_UNORM`)
- Gated by `m_needsSpotShadowPreview` flag (same pattern as directional `m_needsShadowPreview`)
- Display via `ImGui::Image` in inspector under "Shadow Map Preview" collapsing header

New files:
- `engine/shaders/spot_shadow_preview_ps.hlsl`

Files to modify:
- `engine/include/showcase/graphics/SceneRenderer.h` — preview RT, PSO, root sig, render method, flag, accessor
- `engine/src/graphics/SceneRenderer.cpp` — PSO init, `RenderSpotShadowPreview()` implementation
- `engine/CMakeLists.txt` — register new shader
- `editor/src/EditorController.cpp` — ImGui image display, set preview request flag
- `editor/src/EditorApp.cpp` — render loop integration

---

### Tool 4: Spot Light Attenuation Overlay
**Complexity: LOW-MEDIUM | Priority: 4th**

Fullscreen heatmap overlay showing the spot light's combined attenuation factor (distance falloff × angular falloff). This tool is unique to spot lights — directional lights have no attenuation, point lights have only distance falloff. Spot lights are the only type where two independent attenuation factors multiply together, creating a non-intuitive intensity landscape.

Color mapping:
- **Green** (atten 0.5–1.0): High intensity zone — inner cone, close range
- **Yellow** (atten 0.2–0.5): Mid-range or penumbra region
- **Red** (atten 0.01–0.2): Near-zero intensity, outer cone edge
- **Transparent** (atten ≤ 0): Outside cone/range entirely

Implementation:
- New shader `spot_attenuation_overlay_ps.hlsl`:
  - Reconstruct world position from scene depth
  - Compute distance attenuation: `distAtten = saturate(1 - dist/range)²` (matches `mesh_ps.hlsl`)
  - Compute angular attenuation: `spotAtten = saturate((cosAngle - outerCos) / (innerCos - outerCos))` (matches `mesh_ps.hlsl`)
  - Combined: `atten = distAtten * spotAtten`
  - Map `atten` to heatmap color via lerp chain
- Root constants (~28 DWORDs): `invViewProjection` (16), light position (3), range (1), direction (3), innerCos (1), outerCos (1), overlay alpha (1), padding (2)
- SRV: scene depth buffer (t0)
- Same fullscreen post-process pattern as Tool 1

New files:
- `engine/shaders/spot_attenuation_overlay_ps.hlsl`

Files to modify:
- `engine/include/showcase/graphics/SceneRenderer.h` — PSO, render method
- `engine/src/graphics/SceneRenderer.cpp` — PSO init, render implementation
- `engine/CMakeLists.txt` — register new shader
- `editor/include/showcase/editor/ViewportPanel.h` — toggle flag
- `editor/src/EditorApp.cpp` — render loop, console command, config
- `editor/src/EditorController.cpp` — checkbox in inspector

---

### Enhancement: Expanded Debug Stats
**Complexity: LOW | Bundled with Tool 1**

Expand the existing spot light "Shadow Info" section in the inspector to match the richness of directional and point light stats. Pure CPU-side data display, no GPU work.

Additional info to display:
- Light position (world space)
- Direction (forward vector)
- FOV: `2 × outerAngle` degrees (actual shadow frustum field of view)
- Inner / outer angle (degrees)
- Range (meters, = far plane)
- Near plane (0.1)
- Shadow bias
- Texels per meter at mid-range: `resolution / (2 × tan(outerAngle) × range/2)`
- Shadow slot index (-1 if limit reached, 0-7 if assigned)

Files to modify:
- `editor/src/EditorController.cpp` — expand ImGui display under spot light shadow section

---

## Tools NOT Proposed (Rationale)

| Tool | Reason Not Applicable |
|------|-----------------------|
| Cubemap Face ID Overlay | Spot lights use a single 2D shadow map, not a cubemap — no face topology to visualize |
| Depth Precision Heatmap | Spot light FOV is typically narrow (30-90°) with 1024² resolution, giving adequate precision across the cone. The attenuation overlay (Tool 4) provides more spot-light-specific diagnostic value |

## Shared Infrastructure

Tools 1, 4 share the fullscreen post-process pattern: read scene depth → reconstruct world pos → compute per-light metric → output blended color. They share:

- **Selected light tracking**: Use existing `GetSpotShadowIndex(objectId)` to resolve which shadow slot to visualize
- **Root signature pattern**: Root constants (invViewProjection + light params) + scene depth SRV
- **ViewportPanel flag pattern**: `bool m_showXxx` with getter/setter/toggle
- **Console command registration**: `toggle_spot_xxx` in `EditorApp.cpp`
- **Config persistence**: JSON keys `"showSpotXxx"` in `SaveConfig()`/`LoadConfig()`

## Implementation Order

Sequential: implement one tool, verify, then proceed to the next.

### Phase 1: Tool 1 (Spot Shadow Coverage Overlay) + Expanded Stats
- Most actionable debug tool, directly answers "is this pixel shadowed by the spot light?"
- First new shader for spot light overlays, establishes the fullscreen post-process pattern
- Stats expansion is pure CPU work, bundled for efficiency
- Implement → Build → Runtime verify → Phase 2

### Phase 2: Tool 2 (Spot Shadow Frustum Visualization)
- No new shaders required — reuses existing frustum debug infrastructure
- Provides geometric insight into shadow projection volume
- Implement → Build → Runtime verify → Phase 3

### Phase 3: Tool 3 (Spot Shadow Map Preview)
- Inspector panel integration, single render target
- Perspective depth linearization shader
- Implement → Build → Runtime verify → Phase 4

### Phase 4: Tool 4 (Spot Light Attenuation Overlay)
- Piggybacks on the post-process pattern established in Phase 1
- Simple shader (attenuation formula → color map)
- Implement → Build → Runtime verify → Done

## Verification

- Build check after each tool: `scripts/build.bat build Debug`
- Runtime verification: place shadow-casting spot light in scene, select it in inspector
  - Tool 1: Shadowed areas shown in red, lit in green, outside cone in blue; matches visual shadow boundaries
  - Tool 2: Truncated pyramid wireframe aligns with cone gizmo outer boundary and shadow boundaries
  - Tool 3: Grayscale depth image shows scene from light's perspective; depth gradient reflects perspective projection
  - Tool 4: Green hotspot at inner cone near light, yellow penumbra at outer cone, red at range boundary
  - Stats: Values match light component parameters; FOV = 2 × outerAngle; slot index consistent with shadow rendering
