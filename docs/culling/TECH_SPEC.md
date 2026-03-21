# Culling Optimization — Technical Specification

> Reference: [CONTENT_PLAN.md](CONTENT_PLAN.md)

## 1. Overview

### Goal

Demonstrate four culling techniques on a mini-city scene within the engine, allowing real-time toggling of each technique and showing the performance impact through statistics overlay.

### Key Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Occlusion Culling | Software rasterizer (CPU) | Intuitive to visualize, debug-friendly, sufficient for demo |
| glTF loader | tinygltf (header-only) | Lightweight, glTF 2.0 only, matches project's minimal dependency philosophy |
| Spec scope | Unified document | All components in one doc — dependencies visible at a glance |

### Tech Stack (additions)

| Component | Library/API | Integration |
|-----------|-------------|-------------|
| glTF parsing | tinygltf | FetchContent, header-only |
| Image decoding | stb_image (bundled with tinygltf) | Automatic |
| Frustum math | DirectXMath `BoundingFrustum` | Already available via DirectXTK SimpleMath |
| AABB math | DirectXMath `BoundingBox` | Already available via DirectXTK SimpleMath |

---

## 2. Architecture Overview

### System Dependency Graph

```
Engine (all components integrated)
═══════════════════════════════════════════════════════════

                    ┌──────────────┐
                    │ DepthBuffer  │
                    └──────┬───────┘
                           │
              ┌────────────┼────────────┐
              ▼            ▼            ▼
      ┌──────────┐  ┌───────────┐  ┌──────────┐
      │  Camera  │  │  Texture  │  │  Buffer  │
      └────┬─────┘  └─────┬─────┘  └────┬─────┘
           │              │              │
           │         ┌────┴────┐         │
           │         │  Model  │─────────┘
           │         │ (glTF)  │
           │         └────┬────┘
           │              │
           ▼              ▼
      ┌────────────────────────┐
      │    SceneObject/Scene   │
      └────────────┬───────────┘
                   │
                   ▼
      ┌─────────────────────────────────────────┐
      │         SceneRenderer                   │
      │  (root signature, PSO, constant         │
      │   buffers, draw loop, picking)          │
      └────────────┬────────────────────────────┘
                   │
      ┌────────────┼───────────────┐
      ▼            ▼               ▼
 ┌──────────┐ ┌──────────────┐ ┌─────────────────┐
 │FPSCamera │ │EditorControl.│ │ CullingPipeline │
 │Controller│ │(gizmo, UI)   │ │ (to be added)   │
 └──────────┘ └──────────────┘ │ ┌──────┐┌─────┐ │
                               │ │Frust.││Occl.│ │
                               │ └──────┘└─────┘ │
                               │ ┌──────┐┌─────┐ │
                               │ │Back- ││LOD/ │ │
                               │ │face  ││Dist.│ │
                               │ └──────┘└─────┘ │
                               └─────────────────┘
```

### Component Locations

| Module | Contents | Location |
|--------|----------|----------|
| **Graphics** | DepthBuffer, Camera, Texture, Model, Scene, SceneRenderer | `engine/include/showcase/graphics/`, `engine/src/graphics/` |
| **Core** | FPSCameraController, Input, Timer, Window | `engine/include/showcase/core/`, `engine/src/core/` |
| **UI** | EditorController, Viewport, Console, ImGuiLayer | `engine/include/showcase/ui/`, `engine/src/ui/` |
| **Shaders** | mesh_vs.hlsl, mesh_ps.hlsl (+ culling shaders to be added) | `engine/shaders/` |
| **Culling** | FrustumCuller, SoftwareOcclusionCuller, LODSelector, etc. | `engine/src/graphics/` (to be added) |

---

## 3. Engine Infrastructure Additions

### 3.1 Depth Buffer

**Purpose:** Enable depth testing for 3D scene rendering. Currently `SwapChain` only has RTVs.

**File:** `engine/include/showcase/graphics/DepthBuffer.h`, `engine/src/DepthBuffer.cpp`

```cpp
namespace showcase {

class DepthBuffer {
public:
    bool Init(ID3D12Device* device, D3D12MA::Allocator* allocator,
              uint32_t width, uint32_t height,
              DXGI_FORMAT format = DXGI_FORMAT_D32_FLOAT);
    void Shutdown();
    void Resize(ID3D12Device* device, D3D12MA::Allocator* allocator,
                uint32_t width, uint32_t height);

    ID3D12Resource* GetResource() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const;
    DXGI_FORMAT GetFormat() const;

private:
    ComPtr<ID3D12Resource> m_resource;
    ComPtr<D3D12MA::Allocation> m_allocation;
    DescriptorHeap m_dsvHeap;  // 1-descriptor DSV heap
    DXGI_FORMAT m_format = DXGI_FORMAT_D32_FLOAT;
};

} // namespace showcase
```

**Integration with RenderContext:**
- Add `DepthBuffer m_depthBuffer` member to `RenderContext`
- Initialize in `RenderContext::Init()`, resize in `RenderContext::Resize()`
- Add `DepthBuffer& GetDepthBuffer()` accessor
- `Application` passes DSV handle alongside RTV in `OMSetRenderTargets()`
- Clear depth buffer each frame with `ClearDepthStencilView()`

**Implementation notes:**
- Use `D3D12MA::Allocator` for GPU memory allocation (consistent with existing `Buffer` pattern)
- DSV heap: 1 descriptor, non-shader-visible (`D3D12_DESCRIPTOR_HEAP_TYPE_DSV`)
- Resource state: `D3D12_RESOURCE_STATE_DEPTH_WRITE`

---

### 3.2 Camera System

**Purpose:** Provide view/projection matrices and frustum extraction for 3D rendering.

**File:** `engine/include/showcase/graphics/Camera.h`, `engine/src/Camera.cpp`

```cpp
namespace showcase {

class Camera {
public:
    void SetPerspective(float fovY, float aspectRatio, float nearZ, float farZ);
    void SetPosition(const SimpleMath::Vector3& position);
    void SetLookAt(const SimpleMath::Vector3& target, const SimpleMath::Vector3& up);

    void UpdateViewMatrix();

    // Getters
    const SimpleMath::Matrix& GetViewMatrix() const;
    const SimpleMath::Matrix& GetProjectionMatrix() const;
    SimpleMath::Matrix GetViewProjectionMatrix() const;
    const SimpleMath::Vector3& GetPosition() const;
    const SimpleMath::Vector3& GetForward() const;
    const SimpleMath::Vector3& GetRight() const;
    const SimpleMath::Vector3& GetUp() const;
    float GetFOV() const;
    float GetNearZ() const;
    float GetFarZ() const;

    // Frustum
    DirectX::BoundingFrustum GetBoundingFrustum() const;

private:
    SimpleMath::Vector3 m_position = {0, 0, 0};
    SimpleMath::Vector3 m_forward  = {0, 0, 1};
    SimpleMath::Vector3 m_right    = {1, 0, 0};
    SimpleMath::Vector3 m_up       = {0, 1, 0};

    SimpleMath::Matrix m_viewMatrix;
    SimpleMath::Matrix m_projectionMatrix;

    float m_fovY = DirectX::XM_PIDIV4;     // 45 degrees
    float m_aspectRatio = 16.0f / 9.0f;
    float m_nearZ = 0.1f;
    float m_farZ  = 1000.0f;
};

} // namespace showcase
```

**FPS Camera Controller** (engine core — God View will extend or compose with this):

```cpp
// engine/include/showcase/core/FPSCameraController.h
class FPSCameraController {
public:
    void Update(Camera& camera, const Input& input, float deltaTime);

    float moveSpeed = 10.0f;
    float lookSpeed = 0.003f;

private:
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
};
```

- WASD for movement, mouse for look (right-click drag to prevent ImGui conflict)
- Pitch clamped to ±89 degrees

**Frustum extraction:**
- `DirectX::BoundingFrustum::CreateFromMatrix()` from projection matrix
- Transform to world space using inverse view matrix
- Use `BoundingFrustum::Contains(BoundingBox)` for culling tests

---

### 3.3 glTF Model Loading

**Purpose:** Load external 3D models for the city scene with high polygon counts.

**Dependency:** tinygltf via FetchContent

```cmake
# cmake/Dependencies.cmake — add:
FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG        v2.9.5
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(tinygltf)
```

**Data structures:**

```
engine/include/showcase/graphics/Model.h
engine/src/Model.cpp
```

```cpp
namespace showcase {

struct MeshPrimitive {
    Buffer vertexBuffer;
    Buffer indexBuffer;
    uint32_t indexCount = 0;
    uint32_t materialIndex = UINT32_MAX;
    DirectX::BoundingBox localAABB;  // computed at load time
};

struct Mesh {
    std::vector<MeshPrimitive> primitives;
    std::string name;
};

struct Material {
    SimpleMath::Vector4 baseColorFactor = {1, 1, 1, 1};
    uint32_t baseColorTextureIndex = UINT32_MAX;
    // Minimal PBR — expand later if needed
};

struct Model {
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<Texture> textures;
    DirectX::BoundingBox localAABB;  // union of all mesh AABBs
};

class ModelLoader {
public:
    static std::unique_ptr<Model> LoadGLTF(
        const std::string& path,
        ID3D12Device* device,
        D3D12MA::Allocator* allocator,
        CommandList& cmdList,
        DescriptorHeap& srvHeap
    );
};

} // namespace showcase
```

**Vertex format:**

```cpp
struct ModelVertex {
    SimpleMath::Vector3 position;
    SimpleMath::Vector3 normal;
    SimpleMath::Vector2 texCoord;
};
```

**Implementation notes:**
- Parse with `tinygltf::TinyGLTF::LoadASCIIFromFile()` / `LoadBinaryFromFile()`
- Extract positions, normals, texcoords from accessors → create `ModelVertex` array
- Create vertex/index buffers using existing `Buffer::InitAsVertexBuffer()` / `InitAsIndexBuffer()`
- Compute AABB from positions during loading (`DirectX::BoundingBox::CreateFromPoints()`)
- Texture loading delegates to Texture system (section 3.4)
- Upload buffers via a temporary upload heap; execute and wait before returning

---

### 3.4 Texture System

**Purpose:** Load images from glTF into GPU textures with SRV descriptors.

**File:** `engine/include/showcase/graphics/Texture.h`, `engine/src/Texture.cpp`

```cpp
namespace showcase {

class Texture {
public:
    bool InitFromMemory(ID3D12Device* device, D3D12MA::Allocator* allocator,
                        CommandList& cmdList, DescriptorHeap& srvHeap,
                        const uint8_t* data, uint32_t width, uint32_t height,
                        uint32_t channels, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);
    void Shutdown();

    DescriptorHandle GetSRV() const;
    ID3D12Resource* GetResource() const;

private:
    ComPtr<ID3D12Resource> m_resource;
    ComPtr<D3D12MA::Allocation> m_allocation;
    DescriptorHandle m_srvHandle;
};

} // namespace showcase
```

**Implementation notes:**
- Create default heap texture (`D3D12_RESOURCE_DIMENSION_TEXTURE2D`)
- Create upload buffer sized for texture data (row pitch aligned to `D3D12_TEXTURE_DATA_PITCH_ALIGNMENT`)
- Copy via `UpdateSubresources()` helper or manual `CopyTextureRegion()`
- Transition resource state: `COPY_DEST` → `PIXEL_SHADER_RESOURCE`
- Allocate SRV from `RenderContext::GetSrvHeap()`
- If glTF image has 3 channels (RGB), expand to 4 (RGBA) during load

---

### 3.5 Scene / Object Management

**Purpose:** Manage collections of objects with transforms and per-object AABB for culling.

**File:** `engine/include/showcase/graphics/Scene.h`, `engine/src/Scene.cpp`

```cpp
namespace showcase {

struct SceneObject {
    Model* model = nullptr;              // shared model reference (not owned)
    SimpleMath::Matrix worldTransform;
    DirectX::BoundingBox worldAABB;      // model AABB transformed to world space

    // Per-frame culling state
    bool frustumCulled = false;
    bool occlusionCulled = false;
    int lodLevel = 0;                    // 0 = full detail, -1 = distance culled
};

class Scene {
public:
    SceneObject& AddObject(Model* model, const SimpleMath::Matrix& transform);
    void Clear();

    std::vector<SceneObject>& GetObjects();
    const std::vector<SceneObject>& GetObjects() const;
    size_t GetObjectCount() const;

private:
    std::vector<SceneObject> m_objects;
};

} // namespace showcase
```

**AABB world transform:**
- On `AddObject()`, compute `worldAABB` by transforming `model->localAABB` with the world matrix
- Use `BoundingBox::Transform()` from DirectXMath

---

## 4. Culling Systems

All culling systems will be added to the engine (`engine/src/graphics/`) as reusable components.

### 4.1 Frustum Culling

**File:** `engine/include/showcase/graphics/FrustumCuller.h`, `engine/src/graphics/FrustumCuller.cpp`

```cpp
class FrustumCuller {
public:
    // Returns number of objects culled
    uint32_t Cull(const Camera& camera, std::vector<SceneObject>& objects);

private:
    DirectX::BoundingFrustum m_frustum;
};
```

**Algorithm:**
1. Extract `BoundingFrustum` from camera's projection matrix
2. Transform frustum to world space (inverse view)
3. For each object: `frustum.Contains(object.worldAABB)`
   - `DISJOINT` → `frustumCulled = true`
   - `INTERSECTS` or `CONTAINS` → `frustumCulled = false`

**Performance:** O(n) per object. For 10,000 objects this is negligible on CPU.

**Visualization:**
- God View: draw frustum as wireframe lines (12 edges of the truncated pyramid)
- Culled objects rendered as transparent/gray when "Highlight Culled" is ON

---

### 4.2 Back-face Culling

**Implementation:** This is a GPU rasterizer state toggle, not a CPU algorithm.

```cpp
// Two PSOs: one with back-face culling, one without
GraphicsPipelineDesc descCullBack;
descCullBack.rasterizerState.CullMode = D3D12_CULL_MODE_BACK;   // ON (default)

GraphicsPipelineDesc descCullNone;
descCullNone.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;   // OFF
```

**Visualization — back-face highlight shader:**
- Two-pass rendering when "Show Back-faces" is ON:
  1. First pass: render front faces normally
  2. Second pass: render back faces only (`CullMode = FRONT`) with red-tinted shader
- Triangle count comparison: the GPU pipeline statistics query (`D3D12_QUERY_TYPE_PIPELINE_STATISTICS`) provides `CInvocations` (primitives after culling) vs `CPrimitives` (primitives before culling)

**Pipeline statistics query for triangle counting:**

```cpp
// Create query heap
D3D12_QUERY_HEAP_DESC queryDesc = {};
queryDesc.Type = D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;
queryDesc.Count = 1;
device->CreateQueryHeap(&queryDesc, IID_PPV_ARGS(&m_queryHeap));

// In render loop
cmdList->BeginQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_PIPELINE_STATISTICS, 0);
// ... draw calls ...
cmdList->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_PIPELINE_STATISTICS, 0);
cmdList->ResolveQueryData(m_queryHeap.Get(), D3D12_QUERY_TYPE_PIPELINE_STATISTICS,
                           0, 1, m_readbackBuffer.Get(), 0);

// Read back D3D12_QUERY_DATA_PIPELINE_STATISTICS
// .IAPrimitives = input triangles
// .CInvocations = triangles after culling
```

---

### 4.3 Occlusion Culling (Software Rasterizer)

**File:** `engine/include/showcase/graphics/SoftwareOcclusionCuller.h`, `engine/src/graphics/SoftwareOcclusionCuller.cpp`

**Overview:** Render large occluders (buildings) into a low-resolution CPU depth buffer, then test remaining objects' AABBs against this depth buffer.

```cpp
class SoftwareOcclusionCuller {
public:
    void Init(uint32_t width = 256, uint32_t height = 144);  // low res

    // Phase 1: Rasterize occluders into CPU depth buffer
    void RasterizeOccluders(const Camera& camera,
                            const std::vector<SceneObject>& occluders);

    // Phase 2: Test objects against the depth buffer
    // Returns number of objects culled
    uint32_t TestObjects(const Camera& camera,
                         std::vector<SceneObject>& objects);

    // For debug visualization
    const std::vector<float>& GetDepthBuffer() const;
    uint32_t GetWidth() const;
    uint32_t GetHeight() const;

private:
    std::vector<float> m_depthBuffer;
    uint32_t m_width = 256;
    uint32_t m_height = 144;

    void RasterizeTriangle(const SimpleMath::Vector4& v0,
                           const SimpleMath::Vector4& v1,
                           const SimpleMath::Vector4& v2);
    bool TestAABB(const DirectX::BoundingBox& aabb,
                  const SimpleMath::Matrix& viewProj);
};
```

**Algorithm:**

1. **Occluder selection:** Objects flagged as occluders (large buildings). Selection criteria: world AABB surface area above threshold.

2. **Occluder rasterization:**
   - Transform occluder triangles to screen space (viewProj × vertex → clip → NDC → screen)
   - Rasterize triangles into CPU depth buffer using scanline fill
   - Only write depth (no color) — performance optimization
   - Resolution: 256×144 (very low, but sufficient for AABB occlusion testing)

3. **AABB depth test:**
   - Project 8 corners of each object's world AABB to screen space
   - Find screen-space bounding rect of the projected AABB
   - Sample CPU depth buffer at bounding rect → if all samples are closer than AABB's nearest Z, the object is occluded
   - `occlusionCulled = true` if fully occluded

**Performance considerations:**
- Low resolution keeps rasterization cost manageable
- Only rasterize occluders (tens, not thousands)
- AABB test is a few screen-space samples per object
- Can be parallelized per-object with `std::for_each(std::execution::par, ...)`

**Visualization:**
- ImGui window showing the software depth buffer as a grayscale image
- Upload CPU depth buffer to a GPU texture each frame for ImGui rendering

---

### 4.4 LOD / Distance Culling

**File:** `engine/include/showcase/graphics/LODSelector.h`, `engine/src/graphics/LODSelector.cpp`

```cpp
struct LODConfig {
    float lod0MaxDistance = 50.0f;    // full detail
    float lod1MaxDistance = 150.0f;   // medium detail
    float lod2MaxDistance = 300.0f;   // low detail
    float cullDistance = 500.0f;      // beyond this → don't draw at all
};

class LODSelector {
public:
    LODConfig config;

    // Returns number of objects distance-culled (lodLevel = -1)
    uint32_t SelectLOD(const Camera& camera, std::vector<SceneObject>& objects);
};
```

**Algorithm:**
1. For each object, compute distance from camera to object's world AABB center
2. Assign LOD level based on distance thresholds:
   - `< lod0MaxDistance` → LOD 0 (full detail)
   - `< lod1MaxDistance` → LOD 1
   - `< lod2MaxDistance` → LOD 2
   - `≥ cullDistance` → distance culled (`lodLevel = -1`)

**LOD mesh strategy:**
- Option A: glTF models contain multiple mesh primitives at different detail levels
- Option B: Runtime mesh simplification (out of scope — use pre-authored LODs)
- Option C: For demo purposes, scale down and reduce draw if model doesn't have LOD variants

**Visualization:**
- LOD level color overlay: LOD 0 = green, LOD 1 = yellow, LOD 2 = orange, culled = red

---

## 5. Culling Demo Implementation

### 5.1 City Scene Builder

**File:** `engine/include/showcase/graphics/CitySceneBuilder.h`, `engine/src/graphics/CitySceneBuilder.cpp`

```cpp
struct CityConfig {
    int gridSizeX = 20;            // blocks in X
    int gridSizeZ = 20;            // blocks in Z
    float blockSize = 30.0f;       // meters per block
    float streetWidth = 8.0f;      // meters
    int treesPerBlock = 4;
    int carsPerStreet = 2;
    int propsPerBlock = 3;         // benches, street lights, etc.
};

class CitySceneBuilder {
public:
    void Build(Scene& scene, const CityConfig& config,
               const std::vector<Model*>& buildings,
               const std::vector<Model*>& trees,
               const std::vector<Model*>& cars,
               const std::vector<Model*>& props);
};
```

**Placement algorithm:**
1. Create grid of blocks (gridSizeX × gridSizeZ)
2. Each block gets a randomly selected building model
3. Random scale variation (0.8×–1.3×), random Y rotation (0°/90°/180°/270°)
4. Place trees at random positions within block with margin from building
5. Place cars along streets (between blocks)
6. Place props (street lights, benches) at block corners

**Target:** ~5,000–10,000 total objects from a handful of unique models (5–10)

---

### 5.2 Dual Camera System

**File:** `engine/include/showcase/graphics/DualCameraSystem.h`, `engine/src/graphics/DualCameraSystem.cpp`

```cpp
class DualCameraSystem {
public:
    enum class Mode { Player, God };

    void Update(const Input& input, float deltaTime);

    Camera& GetActiveCamera();
    Camera& GetPlayerCamera();   // always available for frustum vis
    Mode GetMode() const;
    void SetMode(Mode mode);

private:
    Camera m_playerCamera;
    Camera m_godCamera;
    FPSCameraController m_playerController;
    // God camera: fixed top-down, orbitable with mouse
    Mode m_mode = Mode::Player;
};
```

**God View specifics:**
- Camera positioned high above the city, looking down
- Orthographic or very wide FOV perspective
- Mouse drag to pan, scroll to zoom
- Player camera's frustum drawn as wireframe in this view

---

### 5.3 Frustum Wireframe Visualization

**File:** `engine/include/showcase/graphics/FrustumVisualizer.h`, `engine/src/graphics/FrustumVisualizer.cpp`

**Rendering the frustum as 12 wireframe edges in God View.**

```cpp
class FrustumVisualizer {
public:
    void Init(ID3D12Device* device, D3D12MA::Allocator* allocator, ...);
    void Render(CommandList& cmdList, const Camera& playerCamera,
                const Camera& viewCamera);  // viewCamera = god camera

private:
    Buffer m_vertexBuffer;   // 8 frustum corner vertices, updated each frame
    Buffer m_indexBuffer;    // 24 indices (12 edges × 2 vertices)
    ComPtr<ID3D12PipelineState> m_pso;  // line list topology
    ComPtr<ID3D12RootSignature> m_rootSig;
};
```

**Frustum corners computation:**
- `BoundingFrustum::GetCorners()` returns 8 world-space corners
- Upload to vertex buffer each frame
- Render as `D3D_PRIMITIVE_TOPOLOGY_LINELIST` with 12 edges

---

### 5.4 ImGui Controls

Based on CONTENT_PLAN.md "쇼케이스 앱 인터랙션" section:

```cpp
struct CullingSettings {
    // Toggles
    bool frustumCullingEnabled = false;   // start OFF for demo
    bool backfaceCullingEnabled = false;
    bool occlusionCullingEnabled = false;
    bool lodCullingEnabled = false;

    // Camera parameters
    float fov = 60.0f;                    // degrees, range 30–120
    float farPlane = 500.0f;              // range 50–2000

    // Visualization
    bool showFrustum = false;
    bool showBackfaces = false;
    bool highlightCulled = false;
    bool showOcclusionBuffer = false;

    // Scene
    int objectCount = 5000;               // range 100–10,000
};

struct CullingStats {
    uint32_t totalObjects = 0;
    uint32_t drawnObjects = 0;
    uint32_t frustumCulledCount = 0;
    uint32_t occlusionCulledCount = 0;
    uint32_t distanceCulledCount = 0;
    uint64_t totalTriangles = 0;
    uint64_t drawnTriangles = 0;
    uint32_t drawCalls = 0;
    float fps = 0.0f;
};
```

**ImGui layout:**
```
┌─ Culling Controls ─────────────────┐
│ [x] Frustum Culling                │
│ [x] Back-face Culling              │
│ [x] Occlusion Culling              │
│ [x] LOD / Distance Culling         │
│                                    │
│ FOV:       [====|=====] 60°        │
│ Far Plane: [====|=====] 500        │
│ Objects:   [====|=====] 5000       │
│                                    │
│ ── Visualization ──                │
│ [ ] Show Frustum                   │
│ [ ] Show Back-faces                │
│ [ ] Highlight Culled               │
│ [ ] Show Occlusion Buffer          │
│                                    │
│ [Player View] [God View]           │
└────────────────────────────────────┘

┌─ Statistics ───────────────────────┐
│ FPS: 60.0                         │
│ Objects: 823 / 5000               │
│ Triangles: 1.2M / 8.5M           │
│ Draw Calls: 823                   │
│ Frustum Culled: 3800              │
│ Occlusion Culled: 320             │
│ Distance Culled: 57               │
└────────────────────────────────────┘
```

---

### 5.5 Statistics Collection

```cpp
class StatsCollector {
public:
    void BeginFrame();
    void RecordDrawCall(uint32_t triangleCount);
    void EndFrame(float deltaTime);

    CullingStats GetStats() const;

private:
    CullingStats m_current;
    CullingStats m_display;  // smoothed for stable display
};
```

- Draw call count: incremented in render loop for each non-culled object
- Triangle count: sum of `indexCount / 3` for each drawn primitive
- GPU triangle count (after back-face culling): from pipeline statistics query (section 4.2)
- FPS: from existing `Timer` class

---

## 6. Shaders

All shaders in `engine/shaders/`. `mesh_vs.hlsl` and `mesh_ps.hlsl` already exist; culling-specific shaders will be added here.

### 6.1 Mesh Rendering (mesh_vs.hlsl, mesh_ps.hlsl)

**Vertex shader:**
```hlsl
cbuffer PerFrame : register(b0) {
    float4x4 viewProjection;
    float3 cameraPosition;
    float padding;
};

cbuffer PerObject : register(b1) {
    float4x4 world;
};

struct VSInput {
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct PSInput {
    float4 position : SV_POSITION;
    float3 worldNormal : NORMAL;
    float2 texCoord : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
};
```

**Pixel shader:**
- Simple directional light + ambient
- Base color from texture (if available) or material color
- No PBR — keep it visually clean for the video

### 6.2 Back-face Highlight (backface_ps.hlsl)

- Used with `CullMode = FRONT` (renders only back faces)
- Outputs solid red/semi-transparent red

### 6.3 Frustum Wireframe (frustum_vs.hlsl, frustum_ps.hlsl)

- Line list rendering
- Solid color (yellow/white) with slight transparency
- Input: 8 frustum corners as vertices

### 6.4 Culled Object Overlay (culled_ps.hlsl)

- When "Highlight Culled" is ON, culled objects are rendered with this shader
- Semi-transparent gray or color-coded by culling reason:
  - Frustum culled: blue tint
  - Occlusion culled: orange tint
  - Distance culled: red tint

### 6.5 LOD Visualization (lod_overlay_ps.hlsl)

- Color overlay based on LOD level
- LOD 0: green, LOD 1: yellow, LOD 2: orange

---

## 7. Implementation Milestones

See [PROGRESS.md](PROGRESS.md) for milestone definitions and current status.

---

## 8. Assets

### Required Models (glTF format)

| Category | Count | Examples | Polygon Target |
|----------|-------|----------|----------------|
| Buildings | 3–5 types | apartment, office, shop, house | 5K–50K tris each |
| Trees | 1–2 types | deciduous, conifer | 2K–10K tris |
| Vehicles | 1–2 types | sedan, truck | 3K–15K tris |
| Props | 2–3 types | street light, bench, mailbox | 500–3K tris |

### Recommended Free Asset Sources

- **Sketchfab** (CC license filter) — high-quality glTF downloads
- **Kenney.nl** — low-poly city/nature packs (free, CC0)
- **Quaternius** — free low-poly packs
- **Polyhaven** — models + textures (CC0)
- **glTF Sample Assets** — official Khronos test models

### LOD Variants

For LOD demonstration, either:
- Source models with multiple LOD levels already authored
- Use Blender's Decimate modifier to generate LOD 1 (50% tris) and LOD 2 (15% tris) from the base model
- Export each LOD as a separate glTF file, load all variants per model type

---

## Appendix: File Structure

```
engine/
├── include/showcase/
│   ├── core/
│   │   ├── Application.h
│   │   ├── FPSCameraController.h
│   │   ├── Input.h
│   │   ├── Timer.h
│   │   └── Window.h
│   ├── graphics/
│   │   ├── Buffer.h
│   │   ├── Camera.h
│   │   ├── DepthBuffer.h
│   │   ├── Model.h
│   │   ├── Scene.h
│   │   ├── SceneRenderer.h
│   │   ├── Texture.h
│   │   ├── CitySceneBuilder.h       (to be added)
│   │   ├── DualCameraSystem.h       (to be added)
│   │   ├── FrustumCuller.h          (to be added)
│   │   ├── FrustumVisualizer.h      (to be added)
│   │   ├── SoftwareOcclusionCuller.h (to be added)
│   │   ├── LODSelector.h            (to be added)
│   │   └── StatsCollector.h         (to be added)
│   └── ui/
│       ├── EditorController.h
│       ├── Viewport.h
│       └── Console.h
├── src/
│   ├── core/
│   │   ├── Application.cpp
│   │   └── FPSCameraController.cpp
│   ├── graphics/
│   │   ├── Camera.cpp
│   │   ├── DepthBuffer.cpp
│   │   ├── Model.cpp
│   │   ├── Scene.cpp
│   │   ├── SceneRenderer.cpp
│   │   └── Texture.cpp
│   └── ui/
│       └── EditorController.cpp
└── shaders/
    ├── mesh_vs.hlsl
    ├── mesh_ps.hlsl
    ├── backface_ps.hlsl             (to be added)
    ├── frustum_vs.hlsl              (to be added)
    ├── frustum_ps.hlsl              (to be added)
    ├── culled_ps.hlsl               (to be added)
    └── lod_overlay_ps.hlsl          (to be added)

docs/culling/
├── CONTENT_PLAN.md
├── TECH_SPEC.md
└── PROGRESS.md
```
