# ShowcaseEngine Coding Convention

This document defines the coding convention for the ShowcaseEngine project. All code — whether new or modified — must follow these rules.

## 1. Naming

| Target | Rule | Example |
|--------|------|---------|
| Types (class / struct / enum) | PascalCase | `SceneRenderer`, `ModelVertex` |
| Functions / methods | PascalCase, verb-first | `SetPosition()`, `GetViewMatrix()` |
| Local variables | camelCase | `deltaTime`, `closestDist` |
| Private / protected members | `m_` + camelCase | `m_viewMatrix`, `m_position` |
| Public struct fields | camelCase, no prefix | `position`, `indexCount` |
| Public class members (rare) | camelCase, no prefix | `cameraMoveSpeed` |
| Static members | `s_` + camelCase | `s_logger` |
| Compile-time constants | `k` + PascalCase | `kMaxObjects`, `kBufferCount` |
| Enum values | PascalCase | `FillMode::Wireframe` |
| Namespace | lowercase | `showcase` |
| CMake targets | PascalCase | `ShowcaseEngine`, `ShowcaseEditor` |
| Files | PascalCase, matching primary type | `Camera.h` / `Camera.cpp` |

## 2. Formatting

- Opening brace on same line (K&R style).
- 4 spaces indentation (no tabs).
- Line length: 120 characters max.
- `if` / `for` / `while`: braces always required, except single-line early return / continue.
- No indentation inside top-level namespace.

```cpp
void Camera::SetPosition(const Vector3& position) {
    m_position = position;
    m_viewDirty = true;
}

void Foo::Update() {
    if (!m_enabled)
        return;

    for (const auto& obj : m_objects) {
        obj.Draw();
    }
}
```

## 3. Headers

- Use `#pragma once` as include guard.
- Include order (blank line between groups):
  1. Corresponding header (in `.cpp` files)
  2. Project headers (`<showcase/...>`)
  3. Third-party headers (`<imgui.h>`, `<tinygltf/...>`)
  4. System / platform headers (`<d3d12.h>`, `<wrl/client.h>`)
  5. Standard library headers (`<vector>`, `<cstdint>`)
- Prefer forward declarations in headers over includes when possible.

## 4. Namespaces

- Top-level namespace: `showcase`.
- Sub-namespaces: introduce when a module's public type count exceeds ~15 or when name collisions arise (e.g., `showcase::audio`, `showcase::physics`).
- Existing `core/` and `graphics/` modules stay in the flat `showcase` namespace for now.

## 5. Code Organization

- Public interface first, private implementation second.
- `struct` for passive data aggregates; `class` for types with invariants or behavior.
- Section banners in `.cpp` files: `// ── Section Name ──────...`
- One primary type per file.

## 6. File Boilerplate

New files follow this template:

**Header (`.h`)**

```cpp
#pragma once

#include <showcase/module/Dependency.h>

#include <third_party.h>

#include <cstdint>

namespace showcase {

class Foo {
public:
    [[nodiscard]] bool Init(...);
    void Shutdown(...);

    void DoSomething();

private:
    int m_bar = 0;
};

} // namespace showcase
```

**Source (`.cpp`)**

```cpp
#include <showcase/module/Foo.h>

#include <showcase/module/Other.h>

namespace showcase {

// ── Init / Shutdown ──────────────────────────────────────────────

bool Foo::Init(...) {
    return true;
}

void Foo::Shutdown(...) {
}

// ── Public ───────────────────────────────────────────────────────

void Foo::DoSomething() {
}

} // namespace showcase
```

## 7. Function Parameters

### 7.1 Parameter Order

Order parameters as follows:

1. Context / system objects first (`RenderContext&`, `CommandList&`)
2. Input parameters next
3. Output parameters last, prefixed with `out` (`Model& outModel`)

### 7.2 Single-Line Declaration Rule

Every function declaration must fit on **one line** (within the 120-character limit). If parameters push the declaration past one line, pack the input parameters into a descriptor struct and pass it by `const&`.

**Rules:**

- **Naming**: `{Purpose}Desc` — e.g., `GLTFLoadDesc`, `PipelineStateDesc`.
- **Output parameters stay separate**: `out`-prefixed parameters remain as individual function parameters, not packed into the descriptor struct.
- **Struct placement**: Define the descriptor struct near the function that uses it (same header).

**Before (violates — multi-line declaration):**

```cpp
static bool LoadGLTF(RenderContext& ctx,
                     const std::string& filepath,
                     Model& outModel);
```

**After:**

```cpp
struct GLTFLoadDesc {
    RenderContext* ctx;
    std::string filepath;
};

static bool LoadGLTF(const GLTFLoadDesc& desc, Model& outModel);
```

## 8. Type Usage

- **`auto`** — allowed only in these cases:
  1. `auto* p = ctx.GetDevice()` — pointer from getter
  2. `auto it = map.find(...)` — iterator
  3. `for (const auto& x : container)` — range-based for
  4. `auto [a, b] = ...` — structured bindings

  Spell out the type in all other cases.

- **`const`** — use aggressively on parameters, methods, and local variables.
- **`T&`** for non-nullable references; **`T*`** for nullable / optional pointers.
- **Ownership**:
  - `ComPtr<>` for COM objects
  - `std::unique_ptr<>` for general ownership
  - Raw `T*` for non-owning observation
- **Integer types**: `uint32_t` for GPU / DX12 API interop; `int` for general integers and sentinels (e.g., `materialIndex = -1`).

## 9. Comments

- Explain **why**, not what.
- Use `//` for inline comments.
- Use `///` one-line descriptions on public API methods whose purpose, return value, or usage constraints are not obvious from the name and signature alone — no `@param` / `@return` / `@brief` tags.
- Section banners: `// ── Name ──────...`

```cpp
/// Transforms ray into local space and tests mesh AABBs.
int PickObject(int mouseX, int mouseY, const Camera& camera) const;

/// Returns the world-space bounding box after applying the current transform.
BoundingBox GetWorldAABB() const;
```

## 10. Error Handling

Two distinct error paths — choose based on cause:

| Situation | Cause | Action | Macro |
|-----------|-------|--------|-------|
| Expected failure | External (file missing, GPU error, bad `HRESULT`) | Log + **debug break** + `return false` | `SE_LOG_ERROR` |
| Programmer mistake | Internal (broken invariant, bad argument, impossible state) | Halt immediately in Debug | `SE_ASSERT` |

### Expected Failures

- Use the `Init()` / `Shutdown()` pattern. `Init()` returns `bool`.
- Use `SE_LOG_*` macros for diagnostics.
- No exceptions.
- `SE_LOG_ERROR` triggers `__debugbreak()` in Debug builds so the debugger stops at the exact failure point with the full call stack. In Release builds it logs only. Cascading breaks (child fails → parent also logs error → breaks again) are expected — the first break is always the root cause.

### Assertions (`SE_ASSERT`)

Debug-only (`NDEBUG` strips it completely). Never put side effects inside assert expressions.

**Where to use:**

- **Preconditions** — validate parameters on function entry:
  ```cpp
  void Renderer::Submit(CommandList& cmdList, const Buffer& vb) {
      SE_ASSERT(vb.GetSize() > 0);
      // ...
  }
  ```
- **Invariants** — verify object state consistency:
  ```cpp
  SE_ASSERT(m_initialized);
  SE_ASSERT(m_descriptorIndex < m_maxDescriptors);
  ```
- **Unreachable paths** — mark code that should never execute:
  ```cpp
  default:
      SE_ASSERT(false && "unexpected enum value");
      break;
  ```
- **Alignment / size constraints** — GPU buffer requirements:
  ```cpp
  SE_ASSERT(bufferSize % 256 == 0);
  static_assert(sizeof(PerObjectData) <= 256);
  ```

**Where NOT to use:**

- User / external input validation (file paths, config values) — use `SE_LOG_ERROR` + `return false`
- GPU / COM API return values (`HRESULT`) — these are expected failures
- Any check that must also run in Release builds

### `static_assert`

Use for compile-time invariants:

- Struct size / alignment: `static_assert(sizeof(PerObjectData) <= 256)`
- Constant constraints: `static_assert(kBufferCount >= 2)`
- Template parameter requirements

## 11. Modern C++ Features

**Use:**

- `enum class`, `constexpr` / `inline constexpr`, default member initializers
- Range-based for, `using` aliases, designated initializers, `std::move`
- `[[nodiscard]]` on `bool Init()` / `Load()` functions and factory functions
- Simple templates for generic containers and type-safe wrappers

**Avoid:**

- SFINAE, recursive templates, `enable_if` chains
- Coroutines, user-defined literals, multiple inheritance

**Use with discretion:**

- `std::optional` — prefer sentinels when the domain allows (e.g., `-1` index)
- Lambdas — ≤5 lines and ≤3 captures OK inline; beyond that, extract to a named function

## 12. Editor UI (ImGui)

- **Shortcut display policy**: Never show keyboard shortcuts inline in menu items or buttons. Instead, display shortcuts as tooltips on mouse hover using `ImGui::IsItemHovered()` + `ImGui::SetTooltip()`.

```cpp
// Good — shortcut shown on hover
if (ImGui::MenuItem("Save")) { SaveScene(); }
if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ctrl+S");

// Bad — shortcut shown inline
if (ImGui::MenuItem("Save", "Ctrl+S")) { SaveScene(); }
```

## 13. HLSL Shaders

Compiled with DXC (Shader Model 6.0), `-WX` warnings-as-errors. Entry point is always `main()`.

### 13.1 File Naming

| Type | Pattern | Example |
|------|---------|---------|
| Vertex shader | `{purpose}_vs.hlsl` | `mesh_vs.hlsl` |
| Pixel shader | `{purpose}_ps.hlsl` | `mesh_ps.hlsl` |
| Compute shader | `{purpose}_cs.hlsl` | `cull_cs.hlsl` |
| Shared header | `{purpose}.hlsli` | `lighting.hlsli` |

- C++ files use PascalCase (matching the primary type), but shaders use **lowercase_underscore** — shader file names identify **purpose + pipeline stage**, not a type. This follows the standard DX12 / game-engine convention.
- `{purpose}` is a short lowercase noun describing what the shader draws or computes.
- Compiled output: `{NAME_WE}_{type}.cso` (e.g. `mesh_vs.hlsl` → `mesh_vs_vs.cso`).

### 13.2 Naming

| Target | Rule | Example |
|--------|------|---------|
| Cbuffer names | PascalCase | `PerFrame`, `PerMaterial` |
| Cbuffer fields | camelCase | `viewProjection`, `baseColorFactor` |
| I/O structs | Stage prefix + `Input` / `Output` | `VSInput`, `VSOutput`, `PSInput` |
| Struct fields | camelCase | `worldPos`, `texCoord` |
| Helper functions | PascalCase | `PristineGrid()`, `LODFade()` |
| Local variables | camelCase | `ndl`, `color` |
| Static constants | UPPERCASE_SNAKE_CASE | `LIGHT_DIR`, `AMBIENT` |
| Padding fields | `_pad` + index | `_pad0`, `_pad1` |
| Textures / samplers | camelCase, descriptive | `baseColorTex`, `linearSampler` |

### 13.3 Formatting

Formatting is enforced automatically by the `slevesque.shader` VSCode extension with `editor.formatOnSave` enabled. Save the file (Ctrl+S) to apply.

- 4-space indentation (no tabs).
- **Allman brace style** — opening brace on the **next** line. This differs from C++ (K&R) because the HLSL formatter applies Allman by default.
- One blank line between cbuffer / struct / function blocks.

```hlsl
Texture2D baseColorTex : register(t0);
SamplerState linearSampler : register(s0);
```

### 13.4 Constant Buffers

Order cbuffers by update frequency, one register per frequency tier:

| Register | Name | Updated |
|----------|------|---------|
| `b0` | `PerFrame` | Once per frame (camera, time, lighting) |
| `b1` | `PerObject` | Once per draw call (world matrix) |
| `b2` | `PerMaterial` | Once per material switch |

Rules:
- **Always declare matrices as `row_major float4x4`.** SimpleMath stores row-major; omitting this qualifier silently produces wrong transforms.
- Pad to 16-byte alignment explicitly with `_pad0`, `_pad1`, etc. Do not rely on implicit padding.
- Keep each cbuffer as small as possible — only include fields the shader actually reads.

```hlsl
cbuffer PerFrame : register(b0)
{
    row_major float4x4 viewProjection;
    float3 cameraPosition;
    float _pad0;                       // explicit 16-byte alignment
};
```

### 13.5 Input / Output Structs

- Vertex shader input: **`VSInput`** with standard semantics (`POSITION`, `NORMAL`, `TEXCOORD`).
- Vertex shader output / pixel shader input: **`VSOutput`** and **`PSInput`** (duplicate the struct in each file until shared headers are introduced).
- Pixel shader output: return `float4 : SV_TARGET` directly from `main()`. Introduce a `PSOutput` struct only when writing to multiple render targets.

```hlsl
struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 worldNorm : NORMAL;
    float3 worldPos : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
};
```

### 13.6 Register Binding

All resources must have explicit register annotations — never rely on implicit assignment.

| Type | Prefix | Slot range |
|------|--------|------------|
| Constant buffers | `b` | `b0` – `b2` (expand as needed) |
| Textures (SRV) | `t` | `t0`+ |
| Samplers | `s` | `s0`+ |
| UAVs | `u` | `u0`+ |

### 13.7 Shared Headers (.hlsli)

Use `.hlsli` files to share struct definitions or utility functions across shader stages. Guard with `#ifndef` / `#define` since HLSL `#pragma once` support is compiler-dependent:

```hlsl
#ifndef SHOWCASE_LIGHTING_HLSLI
#define SHOWCASE_LIGHTING_HLSLI

// Guard name: SHOWCASE_{FILENAME}_HLSLI

float3 LambertDiffuse(float3 normal, float3 lightDir)
{
    return saturate(dot(normal, -lightDir));
}

#endif
```

## 14. Math Abstraction

All math in `engine/` must go through `<showcase/core/Math.h>` or the C++ standard library (`<cmath>`, `<algorithm>`, etc.).

**Allowed:**

- `showcase::` math types and functions defined in `Math.h` (`Vector3`, `Matrix`, `CreateAABB`, `ToRadians`, …)
- `SimpleMath` member functions accessed through the aliased types (e.g., `Vector3::Transform`, `Matrix::Invert`)
- Standard library math (`std::abs`, `std::min`, `std::max`, `<cmath>` functions)

**Prohibited in engine source files:**

- Direct `DirectX::XM*` function calls (`XMVector3TransformCoord`, `XMMatrixLookAtLH`, …)
- DirectX SIMD types (`XMVECTOR`, `XMFLOAT3`, `XMFLOAT4X4`, …)
- `using namespace DirectX;`
- Direct `#include <SimpleMath.h>`, `<DirectXMath.h>`, or `<DirectXCollision.h>` — include `<showcase/core/Math.h>` instead

**When a needed operation has no Math.h wrapper:** add a wrapper to `Math.h` first, then use the wrapper. `Math.h` is the only file permitted to include DirectX math headers directly.

`editor/` code may use DirectX types directly when interfacing with ImGui/ImGuizmo, but should prefer `Math.h` types where practical.

### 13.8 Comments

- **Why, not what** — same principle as C++ code.
- Brief `//` comment above non-trivial helper functions explaining purpose and parameters.
- Inline comments for GPU-specific reasoning (alignment tricks, precision choices, derivative usage).

```hlsl
// Pristine Grid: anti-aliased grid pattern at any scale.
// coord:    world position divided by grid spacing
// uvDeriv:  screen-space derivatives at that scale
float PristineGrid(float2 coord, float2 uvDeriv, float2 lineWidth)
{
    ...
}
```

## 15. Profiling (Tracy)

All profiling goes through the macros in `<showcase/core/Profiler.h>`. Never call Tracy functions directly.

### 15.1 Zone Placement Criteria

Place zones at **architecture boundaries**, not individual functions. The question is "should this stage be visible in the frame timeline?" — not "might this function be slow?"

**Where to place zones (4 categories):**

| Category | Criterion | Examples |
|----------|----------|---------|
| Frame pipeline stages | Functions that appear in the ARCHITECTURE.md frame workflow | `BeginFrame`, `Render`, `EndFrame` |
| CPU-GPU synchronization | Points where CPU waits for GPU or submits GPU work | `WaitForFenceValue`, `Present`, `ExecuteCommandList`, `Flush` |
| I/O and resource creation | File reads, GPU buffer/texture creation, scene serialization | `LoadModel`, `LoadGLTF`, `InitFromFile`, `Deserialize` |
| Init / Shutdown | `Init()` / `Shutdown()` pattern functions (runs once, but startup time matters) | `RenderContext::Init`, `SceneRenderer::Init` |

**Where NOT to place zones:**

- Trivial getter/setter — zone overhead (~50ns) may exceed the function itself
- Per-primitive functions inside draw loops — 64 objects × 6 primitives = 384 calls/frame, adds noise only
- Individual ImGui panel rendering — one zone for the entire ImGui stage is sufficient
- Utility / math functions — `GetExecutableDir`, `ToRadians`, etc.
- One-line wrapper functions that only call an already-instrumented parent

### 15.2 Macro Placement

Place the zone macro on the **first line** of the function body, before any early return:

```cpp
void SceneRenderer::Render(RenderContext& ctx, Camera& camera, Scene& scene, int selectedObjectId) {
    SE_ZONE_SCOPED_C(profile::kColorRendering);
    if (scene.GetObjects().empty())
        return;
    // ...
}
```

For sub-blocks within a function, use a `{}` scope with `SE_ZONE_SCOPED_NC`:

```cpp
{
    SE_ZONE_SCOPED_NC("WaitForFenceValue", profile::kColorGPUSync);
    m_directQueue.WaitForFenceValue(fenceValue);
}
```

Rationale: Tracy zones are RAII. First-line placement measures the full function time including early returns. If a fast path is invisible in Tracy, it causes confusion ("why wasn't this function called?").

### 15.3 Zone Names and Colors

**Names:**

- Whole function → `SE_ZONE_SCOPED` or `SE_ZONE_SCOPED_C(color)` — uses `__FUNCTION__` automatically
- Sub-block within a function → `SE_ZONE_SCOPED_N("Name")` or `SE_ZONE_SCOPED_NC("Name", color)` — PascalCase, `"VerbNoun"` form, max 40 characters

**Colors:** Use the subsystem color constants from `showcase::profile`. When unsure, use colorless (`SE_ZONE_SCOPED`) — Tracy applies its default color.

| Subsystem | Color constant | Usage |
|-----------|---------------|-------|
| Update | `kColorUpdate` (purple) | Input, camera, game logic, picking |
| Rendering | `kColorRendering` (red) | Scene rendering, render target transitions |
| GPU Sync | `kColorGPUSync` (orange) | Fence waits, command submission, Present |
| ImGui | `kColorImGui` (blue) | ImGui frame, editor UI as a whole |
| Asset I/O | `kColorAssetIO` (green) | Model/texture loading, scene serialization |

### 15.4 SE_FRAME_MARK

- Exactly **once per frame**, at the end of the `EditorApp::Run()` main loop
- Same location for both Play mode and Edit mode
- Never add additional calls — multiple calls per frame distort Tracy's frame time graph

### 15.5 SE_PLOT

Display per-frame numeric data as graphs above the Tracy timeline.

| Plot name | Value | Location |
|-----------|-------|----------|
| `"Frame Time (ms)"` | `deltaTime * 1000.0f` | Main loop, right after `Timer::Tick()` |
| `"Draw Calls"` | Number of `DrawIndexedInstanced` calls in the frame | End of `SceneRenderer::Render()` |
| `"Scene Objects"` | `scene.GetObjects().size()` | Start of `SceneRenderer::Render()` |

Naming convention: `"Title Case Name (unit)"` — include units in parentheses when applicable.

### 15.6 SE_MESSAGE

Mark **discrete, non-recurring events** on the Tracy timeline. Used to visually correlate frame time spikes with specific events.

Target events:

- Mode transitions: `SE_MESSAGE_L("Entered Play mode")` / `"Exited Play mode"`
- Scene operations: `SE_MESSAGE_L("Scene loaded")`, `"Scene saved"`, `"New scene created"`
- Resize: `SE_MESSAGE_L("Window resized")`, `"Viewport resized"`
- Asset loading completion: `SE_MESSAGE_L("Model loaded")`

For per-frame recurring data, use `SE_PLOT` instead of `SE_MESSAGE`.

### 15.7 SE_ZONE_TEXT

Attach dynamic context (e.g., file paths) to I/O-related zones:

```cpp
Model* AssetManager::LoadModel(const std::string& source) {
    SE_ZONE_SCOPED_C(profile::kColorAssetIO);
    SE_ZONE_TEXT(source.c_str(), source.size());
    // ...
}
```

Clicking the zone in Tracy reveals which file was being loaded.
