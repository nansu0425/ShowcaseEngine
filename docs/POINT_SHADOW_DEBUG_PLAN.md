# Point Light Shadow Debugging Visualization Tools

## Context

The engine has 4 directional light shadow debug tools (Shadow Info, Shadow Frustum Wireframe, Shadow Map Preview, Shadow Coverage Overlay), all fully implemented. Point light shadows use cubemap depth buffers (512x512 per face, max 6 shadow-casting lights, hard shadows with linear depth reconstruction), but have no dedicated debug visualizations beyond the 3-ring range gizmo.

Point light shadows introduce unique debugging challenges compared to directional:
- **6-face cubemap topology** — seam artifacts at face boundaries, per-face rendering issues
- **Perspective projection** — non-linear depth precision (degrades near far plane)
- **Spherical coverage** — harder to mentally trace shadow boundaries than orthographic frustum
- **Shadow slot limit** — max 6 lights, silently drops shadows when exceeded

## Proposed Tools

### Tool 1: Point Shadow Debug Info Panel
**Complexity: LOW | Priority: 1st**

When a shadow-casting point light is selected in the inspector, display a collapsible "Point Shadow Info" header showing shadow-related metrics. Pure CPU-side data display, no GPU work.

Displayed info:
- Light position (world space)
- Range (= far plane distance)
- Near plane (0.1)
- Shadow map resolution per face (512x512)
- Shadow bias value
- Texel density at mid-range: `resolution / (2 * tan(45°) * range/2)` — indicates if resolution is adequate for the light's range
- Shadow index assignment (-1 if slot limit reached, 0-5 if assigned) — diagnoses "why is my light not casting shadows?"

Implementation:
- Add `PointShadowDebugStats` struct to `SceneRenderer.h` (mirrors `ShadowDebugStats` pattern)
- Populate in `RenderPointShadowPass()` for the currently selected point light
- Add getter `GetPointShadowDebugStats()` to `SceneRenderer`
- Display in `EditorController.cpp` under `LightType::Point` section (after line 880), using `CollapsingHeader("Point Shadow Info")`

Files to modify:
- `engine/include/showcase/graphics/SceneRenderer.h` — struct + getter
- `engine/src/graphics/SceneRenderer.cpp` — populate in `RenderPointShadowPass()`
- `editor/src/EditorController.cpp` — ImGui display under point light inspector

---

### Tool 2: Point Shadow Coverage Overlay
**Complexity: MEDIUM | Priority: 2nd**

Fullscreen post-process overlay for a selected point light. Same visual language as the directional shadow coverage overlay. Color-codes surfaces by shadow state: **Red** = shadowed, **Green** = lit, **Blue tint** = outside light range (no contribution). Blue for "outside range" is especially valuable since the spherical boundary is harder to mentally trace than a directional frustum.

Implementation:
- New shader `point_shadow_overlay_ps.hlsl`:
  - Read scene depth buffer, reconstruct world position via `invViewProjection`
  - Compute distance from world position to light position
  - If distance > range: output blue tint
  - Otherwise: sample cubemap shadow (reuse `CalculatePointShadow` logic from `mesh_ps.hlsl`), lerp red↔green
- New PSO + root signature:
  - Root constants (~24 DWORDs): `invViewProjection` (16), light position (3), range (1), near plane (1), shadow bias (1), overlay alpha (1), shadow index (1)
  - SRV: scene depth buffer (t0), point shadow cubemap (t1)
  - Alpha blend, no depth write
- Add `RenderPointShadowOverlay()` to `SceneRenderer`
- `m_showPointShadowOverlay` flag in `ViewportPanel`
- Console command `/point_shadow_overlay`
- Checkbox in inspector under point light debug section
- Config persistence in `EditorApp`

New files:
- `engine/shaders/point_shadow_overlay_ps.hlsl`

Files to modify:
- `engine/include/showcase/graphics/SceneRenderer.h` — PSO, root sig, render method
- `engine/src/graphics/SceneRenderer.cpp` — PSO init, render implementation
- `engine/CMakeLists.txt` — register new shader
- `editor/include/showcase/editor/ViewportPanel.h` — toggle flag
- `editor/src/EditorApp.cpp` — render loop, console command, config
- `editor/src/EditorController.cpp` — checkbox in inspector

---

### Tool 3: Cubemap Face ID Overlay
**Complexity: LOW-MEDIUM | Priority: 3rd**

Fullscreen overlay color-coding each pixel by which cubemap face is being sampled for the selected point light. 6 distinct colors:
- **+X**: Red, **-X**: Cyan
- **+Y**: Green, **-Y**: Magenta
- **+Z**: Blue, **-Z**: Yellow

Pixels outside the light's range are transparent. This tool diagnoses **cubemap face seam artifacts** — an artifact class unique to point light shadows. Seam locations become instantly visible as color boundaries.

Implementation:
- New shader `point_shadow_face_overlay_ps.hlsl`:
  - Reconstruct world position from scene depth
  - Compute `lightToFrag = worldPos - lightPos`
  - Determine dominant axis: largest component of `abs(lightToFrag)` → face selection
  - Map each of 6 faces to its distinct color
  - No cubemap SRV needed — face selection is purely geometric
- Root signature: root constants (invViewProjection, light position, range, alpha) + scene depth SRV
- Same fullscreen post-process pattern as Tool 2

New files:
- `engine/shaders/point_shadow_face_overlay_ps.hlsl`

Files to modify:
- `engine/include/showcase/graphics/SceneRenderer.h` — PSO, root sig, render method
- `engine/src/graphics/SceneRenderer.cpp` — PSO init, render implementation
- `engine/CMakeLists.txt` — register new shader
- `editor/include/showcase/editor/ViewportPanel.h` — toggle flag
- `editor/src/EditorApp.cpp` — render loop, console command, config
- `editor/src/EditorController.cpp` — checkbox in inspector

---

### Tool 4: Cubemap Face Preview (Cross Layout)
**Complexity: MEDIUM-HIGH | Priority: 4th**

Cross-unfolded layout in the inspector panel showing all 6 cubemap depth faces as grayscale images. Depth is linearized for display (raw cubemap stores perspective depth). Each face labeled (+X, -X, +Y, -Y, +Z, -Z).

Cross layout arrangement:
```
        [+Y]
  [-X]  [+Z]  [+X]
        [-Y]
        [-Z]
```

Implementation:
- New shader `point_shadow_debug_ps.hlsl`:
  - Input: face index (root constant), near plane, far plane
  - Sample cubemap using direction vector derived from UV + face index
  - Linearize depth: `linearDepth = near * far / (far - storedDepth * (far - near))`
  - Normalize to [0,1] and output as grayscale
- Render 6 fullscreen passes to a single preview render target (384x512 or similar cross-layout RT)
- Alternatively: 6 separate small render targets (128x128 each), displayed individually in ImGui
- Gate rendering with `m_needsPointShadowPreview` flag (same pattern as directional preview)
- Display in `EditorController` via `ImGui::Image` calls arranged in cross formation

New files:
- `engine/shaders/point_shadow_debug_ps.hlsl`

Files to modify:
- `engine/include/showcase/graphics/SceneRenderer.h` — preview RT(s), PSO, root sig, render method
- `engine/src/graphics/SceneRenderer.cpp` — PSO init, `RenderPointShadowPreview()` implementation
- `engine/CMakeLists.txt` — register new shader
- `editor/src/EditorController.cpp` — ImGui cross-layout display
- `editor/src/EditorApp.cpp` — render loop integration

---

### Tool 5: Depth Precision Heatmap Overlay
**Complexity: LOW | Priority: 5th**

Fullscreen overlay showing `distance / range` ratio as a heatmap for the selected point light. Visualizes where depth precision problems are most likely to occur.

Color mapping:
- **Green** (0.0–0.5): Close to light, good precision
- **Yellow** (0.5–0.8): Mid-range, acceptable precision
- **Red** (0.8–0.95): Far from light, poor precision
- **Magenta/White** (>0.95): Danger zone — shadow acne highly likely here

Pixels outside the light's range are transparent.

Implementation:
- New shader `point_shadow_depth_heatmap_ps.hlsl`:
  - Reconstruct world position from scene depth
  - Compute `t = distance(worldPos, lightPos) / range`
  - Map `t` to heatmap color via lerp chain
- Can share root signature with Tool 3 (same inputs: root constants + scene depth SRV)
- Same fullscreen post-process pattern

New files:
- `engine/shaders/point_shadow_depth_heatmap_ps.hlsl`

Files to modify:
- `engine/include/showcase/graphics/SceneRenderer.h` — PSO, render method
- `engine/src/graphics/SceneRenderer.cpp` — PSO init, render implementation
- `engine/CMakeLists.txt` — register new shader
- `editor/include/showcase/editor/ViewportPanel.h` — toggle flag
- `editor/src/EditorApp.cpp` — render loop, console command, config
- `editor/src/EditorController.cpp` — checkbox in inspector

---

## Shared Infrastructure

Tools 2, 3, 5 share the same fullscreen post-process pattern: read scene depth → reconstruct world pos → compute per-light metric → output blended color. They share:

- **Selected light tracking**: Add `m_selectedPointShadowIndex` to `SceneRenderer`, set by `EditorController` when a shadow-casting point light is selected. All overlays use this to know which light to visualize.
- **Root signature pattern**: Root constants (invViewProjection + light params) + scene depth SRV. Tools 3 and 5 can share the same root signature (no cubemap SRV needed).
- **ViewportPanel flag pattern**: `bool m_showXxx` with getter/setter/toggle, same as `m_showShadowFrustum`
- **Console command registration**: `/point_shadow_xxx` in `EditorApp.cpp`
- **Config persistence**: JSON keys `"showPointShadowXxx"` in `SaveConfig()`/`LoadConfig()`

## Implementation Order

Sequential: implement one tool, verify, then proceed to the next.

### Phase 1: Tool 1 (Point Shadow Debug Info)
- No new shaders, pure CPU-side data display
- Builds `PointShadowDebugStats` infrastructure + selected light tracking
- Implement → Build → Runtime verify → Phase 2

### Phase 2: Tool 2 (Point Shadow Coverage Overlay)
- Most actionable debug tool, directly answers "is this pixel shadowed?"
- First new shader, establishes fullscreen post-process pattern for point shadows
- Implement → Build → Runtime verify → Phase 3

### Phase 3: Tool 3 (Cubemap Face ID Overlay)
- Diagnoses cubemap-specific seam artifacts
- Simple shader (dominant axis selection), reuses established pattern
- Implement → Build → Runtime verify → Phase 4

### Phase 4: Tool 4 (Cubemap Face Preview)
- Most complex — cross-layout ImGui rendering + per-face depth linearization
- Implement → Build → Runtime verify → Phase 5

### Phase 5: Tool 5 (Depth Precision Heatmap)
- Piggybacks on Tool 3's root signature and pattern
- Simplest shader (distance ratio → color map)
- Implement → Build → Runtime verify → Done

## Verification

- Build check after each tool: `scripts/build.bat build Debug`
- Runtime verification: place shadow-casting point light in scene, select it in inspector
  - Tool 1: Collapsing header shows reasonable metrics, shadow index matches expectations
  - Tool 2: Shadowed areas shown in red, lit in green, outside range in blue
  - Tool 3: 6 distinct color regions visible, boundaries align with cubemap face edges
  - Tool 4: 6 grayscale faces show expected geometry from each axis direction
  - Tool 5: Green near light, red near range boundary, magenta at extreme range
