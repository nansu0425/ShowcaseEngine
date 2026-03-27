# Shadow Debugging Visualization Tools

## Context

Currently ShowcaseEngine has directional light shadow mapping (2048x2048, 3x3 PCF) implemented, but no tools exist to debug or visually inspect shadows. Visualization tools are needed to diagnose shadow artifacts (acne, peter panning, insufficient resolution, etc.) and can also serve as visual material for YouTube content showing viewers "how shadows actually work."

## Proposed Tools

### Tool 1: Shadow Debug Info Overlay
**Complexity: LOW | Priority: 1st**

Text overlay on the viewport displaying shadow-related metrics. Same pattern as the FPS overlay.

Displayed info:
- Light direction vector
- Shadow frustum dimensions (width x height x depth, in meters)
- Texel density (texels/meter) — indicator of whether resolution is sufficient
- Depth bias / slope-scaled bias values
- Shadow map resolution

Implementation:
- Add `ShadowDebugStats` struct to `SceneRenderer`, populate in `RenderShadowPass()`
- Expose AABB extents from `ComputeDirectionalLightVP()`
- Render text via `ImDrawList` in `ViewportPanel` or `EditorController` (bottom-left corner)
- Console command `/shadow_info`, Tools menu toggle, config persistence

Files to modify:
- `engine/include/showcase/graphics/SceneRenderer.h` — stats struct, getter
- `engine/src/graphics/SceneRenderer.cpp` — stats computation, frustum data exposure
- `editor/src/EditorController.cpp` — overlay rendering
- `editor/src/EditorApp.cpp` — console command, menu, config

---

### Tool 2: Shadow Frustum Wireframe
**Complexity: LOW | Priority: 2nd**

Visualize the shadow map's orthographic frustum as a wireframe box in the 3D viewport. Reuses existing light gizmo `DrawLine3D()` + `WorldToScreen()` pattern.

Implementation:
- Compute world-space corners by inverting `lightViewProj` and transforming 8 NDC corners
- Expose as `ShadowFrustumDebugData { Vector3 corners[8]; bool valid; }` struct
- Draw 12 edges in `EditorController` with yellow/orange lines
- Console command `/shadow_frustum`, Tools menu toggle

Files to modify:
- `engine/include/showcase/graphics/SceneRenderer.h` — frustum corners struct, getter
- `engine/src/graphics/SceneRenderer.cpp` — inverse matrix computation, corner storage
- `editor/include/showcase/editor/EditorController.h` — toggle member
- `editor/src/EditorController.cpp` — wireframe drawing
- `editor/src/EditorApp.cpp` — console command, menu, config

---

### Tool 3: Shadow Map Preview Panel
**Complexity: MEDIUM | Priority: 3rd**

Display the shadow depth map as a grayscale image in a separate ImGui panel. Since D32_FLOAT cannot be displayed directly in ImGui, a depth-to-color conversion shader + intermediate R8G8B8A8 render target is required.

Implementation:
- New shader `shadow_debug_ps.hlsl` — sample depth, output as grayscale (already linear for ortho)
- Reuse existing `outline_vs.hlsl` (fullscreen triangle, no vertex buffer)
- Add 512x512 `RenderTarget m_shadowPreviewRT` to `SceneRenderer`, with dedicated root signature + PSO
- New panel class `ShadowDebugPanel` — display preview RT via `ImGui::Image()`
- Console command `/shadow_map`, Tools menu toggle

New files:
- `engine/shaders/shadow_debug_ps.hlsl`
- `editor/include/showcase/editor/ShadowDebugPanel.h`
- `editor/src/ShadowDebugPanel.cpp`

Files to modify:
- `engine/include/showcase/graphics/SceneRenderer.h` — preview RT, PSO, render method
- `engine/src/graphics/SceneRenderer.cpp` — preview rendering implementation
- `engine/CMakeLists.txt` — register new shader
- `editor/CMakeLists.txt` — add new source files
- `editor/src/EditorApp.cpp` — panel wiring, console command, menu, config

---

### Tool 4: Shadow Coverage Overlay
**Complexity: HIGH | Priority: 4th**

Semi-transparent color overlay on the viewport scene showing shadow regions. Red = fully shadowed, Green = fully lit.

Implementation:
- New shader `shadow_overlay_ps.hlsl` — reconstruct world position from scene depth buffer, recompute shadow factor, output color
- Alpha blend onto viewport render target (same pattern as outline pass)
- Requires viewport DepthBuffer SRV transition (DEPTH_WRITE -> PIXEL_SHADER_RESOURCE)
- Pass inverse view-projection matrix via root constants

New files:
- `engine/shaders/shadow_overlay_ps.hlsl`

Files to modify:
- `engine/include/showcase/graphics/SceneRenderer.h` — overlay PSO, root signature
- `engine/src/graphics/SceneRenderer.cpp` — overlay rendering, resource barriers
- `engine/CMakeLists.txt` — register new shader
- `editor/src/EditorApp.cpp` — console command, menu, config

---

## Shared Infrastructure

Unified toggle management for all tools:

```cpp
// SceneRenderer.h
struct ShadowDebugFlags {
    bool showStats = false;
    bool showFrustum = false;
    bool showPreview = false;
    bool showOverlay = false;
};
```

## Implementation Order

Sequential: implement one tool, verify, then proceed to the next.

### Phase 1: Tool 1 (Shadow Debug Info)
- No new shaders, builds `ShadowDebugStats` infrastructure
- Implement -> Build -> Runtime verify -> Phase 2

### Phase 2: Tool 2 (Shadow Frustum Wireframe)
- No new shaders, exposes frustum corners
- Implement -> Build -> Runtime verify -> Phase 3

### Phase 3: Tool 3 (Shadow Map Preview Panel)
- First new shader, new panel
- Implement -> Build -> Runtime verify -> Phase 4

### Phase 4: Tool 4 (Shadow Coverage Overlay)
- Most complex, requires world position reconstruction
- Implement -> Build -> Runtime verify -> Done

## Verification

- Build check after each tool: `scripts/build.bat build Debug`
- Runtime verification: toggle each tool in Sponza scene with directional light + castShadow enabled
  - Tool 1: Text overlay displays reasonable metrics
  - Tool 2: Frustum box aligns with scene shadow coverage area
  - Tool 3: Depth map preview shows scene geometry as white/black grayscale
  - Tool 4: Shadowed areas shown in red, lit areas in green
