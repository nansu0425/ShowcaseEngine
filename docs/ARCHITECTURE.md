# ShowcaseEngine — Architecture

> A C++20 / DirectX 12 / ImGui game engine for demonstrating game tech concepts in YouTube content.
> Target audience: **non-developers** — viewers who have never written code.
>
> For build instructions and coding conventions, see [CLAUDE.md](../CLAUDE.md).

---

## 1. High-Level Architecture

The project is split into two CMake targets with a strict dependency direction:

```
┌──────────────────────────────────────────────┐
│              ShowcaseEditor (exe)             │
│                                              │
│  EditorApp          ViewportPanel            │
│  EditorController   ImGuiLayer               │
│  Console            AssetBrowserPanel        │
│                                              │
│  Links: ImGui (docking), ImGuizmo            │
└──────────────────┬───────────────────────────┘
                   │ links
┌──────────────────┴───────────────────────────┐
│         ShowcaseEngine::Core (static lib)     │
│                                              │
│  ┌───────────┐     ┌──────────────────────┐  │
│  │   core/   │     │     graphics/        │  │
│  │           │     │                      │  │
│  │ Window    │     │ RenderContext        │  │
│  │ Input     │     │ SceneRenderer        │  │
│  │ Timer     │     │ Scene  Camera        │  │
│  │ Log       │     │ Device SwapChain     │  │
│  │           │     │ Buffer Texture       │  │
│  │           │     │ Model  ...           │  │
│  └───────────┘     └──────────────────────┘  │
│                                              │
│  Links: d3d12, dxgi, D3D12MA, spdlog,       │
│         DirectXTK12 (SimpleMath), tinygltf   │
└──────────────────────────────────────────────┘
```

**Key principles:**
- The engine has **zero ImGui dependency**. All UI lives in the editor.
- **Dependency policy:** The editor depends only on **ImGui/ImGuizmo** (external) and **engine public API**. All other external libraries (spdlog, tinygltf, D3D12MA, etc.) and D3D12 APIs are owned by the engine, which exposes abstractions for editor use (e.g., `LogListener` for log capture, `Platform.h` for OS utilities, `Key::` constants for input). Win32 APIs may be used directly in the editor where needed — the editor is a Windows-native application and the engine–editor boundary exists to keep the engine ImGui-free, not to abstract the OS from the editor.
- **Accepted boundary exceptions:** `ImGuiLayer` is the designated platform integration point where ImGui backends (`imgui_impl_dx12`) necessarily touch D3D12 types.
- **Entry point:** `editor/src/Main.cpp` contains the `WinMain` entry point directly.

---

## 2. Module Breakdown

### 2.1 Core Module — `engine/include/showcase/core/`

| Class | File | Purpose |
|-------|------|---------|
| `Window` | `Window.h` | Win32 window creation, message pump, resize callback |
| `Input` | `Input.h` | Keyboard and mouse state polling per frame; `Key::` constants |
| `JsonDocument` | `JsonDocument.h` | JSON serialization wrapper (pimpl over nlohmann/json) |
| `Timer` | `Timer.h` | Frame delta time and FPS calculation |
| `Log` | `Log.h` | spdlog wrapper with `SE_LOG_*` macros |
| `LogListener` | `LogListener.h` | Abstract log listener interface (`LogLevel`, `LogMessage`) for non-spdlog consumers |
| `Platform` | `Platform.h` | OS utilities (`GetExecutableDir()`, `SleepMs()`, `ShowErrorDialog()`, `OpenFileDialog()`, `SaveFileDialog()`) |

### 2.2 Graphics Module — `engine/include/showcase/graphics/`

**Device layer** — low-level DirectX 12 abstractions:

| Class | File | Purpose |
|-------|------|---------|
| `Device` | `Device.h` | D3D12 device, DXGI factory, D3D12MA allocator, debug layer |
| `CommandQueue` | `CommandQueue.h` | Direct command queue with fence-based synchronization |
| `CommandList` | `CommandList.h` | Command allocator + graphics command list |
| `SwapChain` | `SwapChain.h` | Triple-buffered swap chain (BUFFER_COUNT=3) with RTVs |
| `FrameResource` | `FrameResource.h` | Per-frame command allocators (3) and fence values |

**Resource management:**

| Class | File | Purpose |
|-------|------|---------|
| `Buffer` | `Buffer.h` | Vertex, index, and constant buffer abstraction via D3D12MA |
| `Texture` | `Texture.h` | GPU texture with upload staging and SRV descriptor |
| `DepthBuffer` | `DepthBuffer.h` | Depth-stencil resource with dedicated DSV heap |
| `DescriptorHeap` | `DescriptorHeap.h` | Shader-visible SRV heap (256 slots) with index allocator |
| `ShaderManager` | `ShaderManager.h` | Loads and caches compiled shader bytecode (.cso files) |

**Pipeline:**

| Class | File | Purpose |
|-------|------|---------|
| `RootSignature` | `RootSignature.h` | Root signature builder helper |
| `PipelineState` | `PipelineState.h` | Graphics PSO builder helper |
| `RenderTarget` | `RenderTarget.h` | Render target abstraction |
| `OffscreenTarget` | `OffscreenTarget.h` | Off-screen render-to-texture with depth, used by viewport |

**Scene:**

| Class | File | Purpose |
|-------|------|---------|
| `Camera` | `Camera.h` | View/projection matrices, frustum extraction |
| `Model` | `Model.h` | Meshes, materials, textures, local AABB; glTF loading via `ModelLoader` |
| `Scene` | `Scene.h` | Flat collection of `SceneObject` with auto-incrementing IDs |
| `SceneRenderer` | `SceneRenderer.h` | Root signature, PSO, constant buffers, draw loop, object picking |

### 2.3 Editor Module — `editor/include/showcase/editor/`

| Class | File | Purpose |
|-------|------|---------|
| `EditorApp` | `EditorApp.h` | Top-level orchestrator: owns all subsystems, runs main loop, scene document management (new/open/save) |
| `ViewportPanel` | `ViewportPanel.h` | 3D viewport: owns `OffscreenTarget` and `Camera`, handles FPS camera |
| `ImGuiLayer` | `ImGuiLayer.h` | ImGui context init, DX12 backend, frame begin/end |
| `Console` | `Console.h` | Log viewer (via `LogListener`), command system, circular buffer (2048 entries) |
| `EditorController` | `EditorController.h` | Object picking, ImGuizmo gizmos (W/E/R), Scene Hierarchy + Inspector panels |
| `AssetBrowserPanel` | `AssetBrowserPanel.h` | Asset directory browser, import flow, add-to-scene |

---

## 3. RenderContext — The Central Hub

`RenderContext` aggregates all DirectX 12 infrastructure into a single object passed by reference to any system that renders.

```
┌─────────────────────────────────────────────┐
│              RenderContext                   │
│                                             │
│  ┌──────────┐  ┌──────────────┐             │
│  │  Device   │  │ CommandQueue │             │
│  │ (ID3D12   │  │ (direct +   │             │
│  │  Device,  │  │  fence)     │             │
│  │  D3D12MA) │  └──────────────┘             │
│  └──────────┘                               │
│  ┌──────────┐  ┌──────────────┐             │
│  │ SwapChain│  │ FrameResource│             │
│  │ (3 back  │  │ (3 cmd       │             │
│  │  buffers)│  │  allocators) │             │
│  └──────────┘  └──────────────┘             │
│  ┌──────────────┐  ┌──────────────┐         │
│  │DescriptorHeap│  │ ShaderManager│         │
│  │ (SRV, 256)   │  │ (CSO cache)  │         │
│  └──────────────┘  └──────────────┘         │
│  ┌──────────────┐  ┌──────────────┐         │
│  │ CommandList   │  │ DepthBuffer  │         │
│  └──────────────┘  └──────────────┘         │
│                                             │
│  m_currentFrameIndex  (0, 1, 2)             │
└─────────────────────────────────────────────┘
```

### Lifecycle

| Method | What it does |
|--------|-------------|
| `Init(hwnd, w, h)` | Creates device, queue, swap chain, descriptor heap, frame resources, command list, depth buffer |
| `BeginFrame()` | Waits on frame fence, resets command allocator and command list, binds SRV heap |
| `BeginBackBufferPass(clearColor)` | Transitions back buffer to render target, sets RTV + DSV, clears |
| `EndBackBufferPass()` | Transitions back buffer to present state |
| `EndFrame()` | Closes command list, executes on queue, presents swap chain, signals fence, advances frame index |
| `Resize(w, h)` | Flushes queue, resizes swap chain buffers and depth buffer |
| `Shutdown()` | Flushes queue, shuts down all owned subsystems in reverse order |

---

## 4. Rendering Pipeline — Frame Walkthrough

One complete frame in `EditorApp::Run()`:

```
EditorApp::Run()
│
├── Timer::Tick()                              // delta time, FPS
├── Input::Update()                            // keyboard + mouse state
├── ViewportPanel::UpdateCamera()              // WASD + right-click look
├── EditorController::Update()                 // picking, gizmo shortcuts
│
├── RenderContext::BeginFrame()                // fence wait, allocator reset
│
│   ┌── Phase 1: Scene → OffscreenTarget ──────────────────┐
│   │                                                       │
│   ├── ViewportPanel::BeginRender()           // transition offscreen → RT
│   ├── SceneRenderer::Render()                // set root sig, PSO
│   │   ├── Update per-frame CB (VP matrix)
│   │   ├── For each SceneObject:
│   │   │   ├── Update per-object CB at offset (world matrix)
│   │   │   ├── Update per-material CB at offset (color, texture flag)
│   │   │   ├── Set root CBVs + SRV descriptor table
│   │   │   └── DrawIndexedInstanced()
│   │   └── (selected object gets highlight tint via per-material CB)
│   ├── ViewportPanel::EndRender()             // transition offscreen → SRV
│   │                                                       │
│   └───────────────────────────────────────────────────────┘
│
│   ┌── Phase 2: ImGui → Back Buffer ──────────────────────┐
│   │                                                       │
│   ├── RenderContext::BeginBackBufferPass()    // transition backbuffer → RT
│   ├── ImGuiLayer::BeginFrame()               // ImGui::NewFrame()
│   ├── EditorApp::RenderMainMenuBar()        // File menu (New/Open/Save)
│   ├── DockSpace setup (one-time layout)
│   │   ├── Viewport panel    (center, 75%)
│   │   ├── Scene Hierarchy   (right-top)
│   │   ├── Inspector         (right-bottom)
│   │   └── Console           (bottom, 25%)
│   ├── ViewportPanel::OnImGui()               // ImGui::Image(offscreen SRV)
│   ├── EditorController::RenderUI()           // hierarchy, inspector, gizmo
│   ├── Console::Render()                      // log entries
│   ├── ImGuiLayer::EndFrame()                 // record ImGui draw commands
│   ├── RenderContext::EndBackBufferPass()     // transition backbuffer → present
│   │                                                       │
│   └───────────────────────────────────────────────────────┘
│
└── RenderContext::EndFrame()                  // execute, present, signal fence
```

### Why two-phase rendering?

ImGui with docking **owns the back buffer** — it needs to composite multiple panels. The 3D scene renders to an `OffscreenTarget` (a separate render target texture), which the viewport panel displays as an `ImGui::Image`. This allows the 3D viewport to be a resizable, dockable ImGui window.

### Triple buffering

- **SwapChain**: 3 back buffers (swap chain presents while GPU renders to another)
- **FrameResource**: 3 command allocators, one per frame-in-flight, with fence-based synchronization
- Frame index cycles `0 → 1 → 2 → 0 → ...`

---

## 5. Constant Buffer Strategy

### The aliasing problem

A single upload-heap constant buffer rewritten per draw call causes **GPU aliasing** (flickering) — the GPU may still be reading from a region that the CPU is overwriting.

### Solution: offset-based writes

Each `SceneObject` gets its own **256-byte-aligned slot** within a shared upload buffer. `Buffer::UpdateDataAtOffset()` writes to `objectIndex * 256`.

```
Per-Object CB (upload heap, persistently mapped):
┌───────────┬───────────┬───────────┬───────────┬───
│  Object 0 │  Object 1 │  Object 2 │  Object 3 │ ...
│   256 B   │   256 B   │   256 B   │   256 B   │
└───────────┴───────────┴───────────┴───────────┴───
 offset = 0   offset = 256  offset = 512  offset = 768
```

The same pattern applies to the per-material constant buffer.

### Root signature layout

```
Root[0]  CBV  b0   Per-frame data (view-projection matrix, camera position)
Root[1]  CBV  b1   Per-object data (world matrix) — offset per object
Root[2]  CBV  b2   Per-material data (base color, texture flag) — offset per object
Root[3]  Table     SRV t0  Base color texture (or default white)
         Static sampler s0  Linear wrap
```

Max objects: `kMaxObjects = 64`.

---

## 6. Scene Management

### Scene graph

The engine uses a **flat scene graph** — `Scene` holds a `vector<SceneObject>` with no parent-child hierarchy.

```cpp
struct SceneObject {
    uint32_t id;               // auto-incrementing ID
    string name;
    string modelSource;        // model reference ("builtin:cube", "file:path.gltf")
    Model* model;              // shared reference (not owned), resolved at runtime
    Vector3 position;          // local-space position
    Vector3 rotation;          // Euler degrees (Y → X → Z order)
    Vector3 scale;
    Matrix worldTransform;     // computed via RecomputeWorldTransform()
    BoundingBox worldAABB;     // model AABB transformed to world space
    bool frustumCulled;
    bool occlusionCulled;
    int lodLevel;
};
```

### Scene serialization

Scenes are saved/loaded as `.scene` JSON files. `Scene::Serialize()` / `Scene::Deserialize()` handle object data, while `EditorApp` manages file I/O and injects editor-specific sections:

- **Save** serializes each object's `name`, `modelSource`, `position`, `rotation`, `scale`, plus an optional `camera` section (editor viewport position/yaw/pitch)
- **Load** deserializes into `SceneObject` structs with `model = nullptr`; the editor resolves `modelSource` strings back to `Model*` pointers via a model registry. Editor camera is restored from the `camera` section if present
- **Model sources**: `"builtin:cube"` for procedural geometry, `"file:relative/path.gltf"` for glTF assets
- **Editor UI**: File menu bar (New Scene, Open, Save, Save As) with Ctrl+N/O/S/Shift+S shortcuts
- **Dirty tracking**: `EditorController` notifies `EditorApp` on gizmo/inspector changes; window title shows `*` for unsaved state

### Object picking

`SceneRenderer::PickObject()` performs CPU-side **ray-AABB intersection in local space**:
1. Convert mouse coordinates to NDC via inverse view-projection matrix
2. Create ray in world space
3. For each object: transform ray into local space, test against local AABB
4. Return closest hit object ID

Testing in local space correctly handles rotated objects (world-space AABB is axis-aligned and can be inaccurate for rotated geometry).

### Data model

```
Model (shared asset)
├── vector<Mesh>
│   └── vector<MeshPrimitive>
│       ├── Buffer vertexBuffer   (ModelVertex: position, normal, texCoord)
│       ├── Buffer indexBuffer
│       ├── uint32_t indexCount
│       ├── int materialIndex
│       └── BoundingBox localAABB
├── vector<Material>
│   ├── Vector4 baseColorFactor
│   └── int baseColorTextureIndex
├── vector<Texture>
└── BoundingBox localAABB         (union of all mesh AABBs)
```

---

## 7. Key Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Engine-Editor split | Static lib + executable | Engine stays ImGui-free, independently testable |
| Scene graph | Flat `vector<SceneObject>` | Sufficient for demos; avoids hierarchy complexity |
| GPU memory allocation | D3D12MA v2.0.1 | Avoids manual heap management and placement logic |
| Math library | SimpleMath (DirectXTK12) | Familiar API, header-only, wraps DirectXMath |
| Coordinate system | Left-handed | Matches DirectX conventions; use `*LH` matrix helpers |
| Shader model | SM 6.0 via DXC | Modern HLSL features without requiring SM 6.6+ |
| Object picking | CPU ray-AABB (local space) | Simple, no GPU readback latency, handles rotation |
| Constant buffers | Offset-based upload heap | Prevents aliasing/flickering with per-object slots |
| glTF loading | tinygltf (header-only) | Lightweight, glTF 2.0 only, minimal dependencies |
| Editor UI | ImGui docking + ImGuizmo | Rapid iteration, built-in docking layout, 3D gizmos |
| Unit system | 1 unit = 1 meter | Intuitive for non-dev audience, matches glTF standard, clean physics values (gravity = 9.8) |
| Scene serialization | JSON via `JsonDocument` | Human-readable `.scene` files; model references via `modelSource` strings; editor camera saved per-scene |

---

## 8. Dependencies

All third-party libraries are fetched via CMake `FetchContent` (see `cmake/Dependencies.cmake`).

| Library | Version | Purpose | Integration |
|---------|---------|---------|-------------|
| D3D12MemoryAllocator | v2.0.1 | GPU memory allocation | `FetchContent_MakeAvailable` |
| Dear ImGui | v1.91.8-docking | Editor UI framework | Manual static lib target |
| ImGuizmo | master | 3D transform gizmos | Manual static lib target |
| DirectXTK12 | oct2025 | SimpleMath (header-only) | `FetchContent_Populate` → interface lib |
| spdlog | v1.15.0 | Logging framework | `FetchContent_MakeAvailable` |
| nlohmann/json | v3.11.3 | JSON parsing | `FetchContent_MakeAvailable` |
| tinygltf | v2.9.5 | glTF 2.0 loader (header-only) | `FetchContent_Populate` → interface lib |

**System libraries** (Windows SDK): `d3d12.lib`, `dxgi.lib`, `dxguid.lib`, `d3dcompiler.lib`

### Shader compilation

HLSL shaders are compiled to SM 6.0 bytecode via DXC at build time using `target_compile_shaders()` (defined in `cmake/ShaderCompilation.cmake`). Output: `${CMAKE_BINARY_DIR}/shaders/{name}_{type}.cso`. At runtime, `ShaderManager` loads `.cso` files relative to the executable directory.
