# Culling Optimization — Progress

> All engine infrastructure (Milestones 1–2) and base scene rendering (Milestone 3 partial) have been integrated into the engine core. Culling-specific features (Milestones 4–8) will be added directly to the engine.

## Milestone 1: Foundation ✅

> Deliverable: Camera flies through empty scene with depth testing

- [x] DepthBuffer (`engine/src/graphics/DepthBuffer.cpp`)
- [x] Camera + FPS controller (`engine/src/graphics/Camera.cpp`, `engine/src/core/FPSCameraController.cpp`)
- [x] Basic mesh shader (`engine/shaders/mesh_vs.hlsl`, `mesh_ps.hlsl`)

## Milestone 2: Model Loading ✅

> Deliverable: Load and render a single glTF model

- [x] tinygltf integration (Dependencies.cmake)
- [x] Texture system (`engine/src/graphics/Texture.cpp`)
- [x] ModelLoader — glTF → GPU buffers (`engine/src/graphics/Model.cpp`)
- [x] Textured mesh shader

## Milestone 3: Scene

> Deliverable: Mini city with thousands of objects, no culling

- [x] SceneObject / Scene (`engine/src/graphics/Scene.cpp`)
- [x] Per-object world transform + constant buffer (SceneRenderer)
- [ ] CitySceneBuilder
- [ ] Object count slider

## Milestone 4: Frustum Culling

> Deliverable: Frustum culling with God View visualization

- [ ] FrustumCuller
- [ ] Dual camera system (Player / God)
- [ ] Frustum wireframe visualizer
- [ ] Highlight Culled visualization
- [ ] FOV / Far Plane sliders

## Milestone 5: Back-face Culling

> Deliverable: Back-face culling toggle with triangle count display

- [ ] CullMode PSO toggle
- [ ] Back-face highlight shader
- [ ] Pipeline statistics query (triangle counting)

## Milestone 6: Occlusion Culling

> Deliverable: Occlusion culling with debug depth buffer view

- [ ] SoftwareOcclusionCuller
- [ ] Occluder selection logic
- [ ] Occlusion depth buffer visualization (ImGui)

## Milestone 7: LOD / Distance Culling

> Deliverable: LOD switching and distance culling

- [ ] LODSelector
- [ ] LOD visualization overlay
- [ ] Distance culling

## Milestone 8: Polish

> Deliverable: Complete showcase ready for video recording

- [ ] Statistics overlay (final layout)
- [ ] ImGui control panel (final layout)
- [ ] Before/After comparison mode
- [ ] Performance tuning
- [ ] Edge case demonstrations (pop-in, frustum boundary flicker)
