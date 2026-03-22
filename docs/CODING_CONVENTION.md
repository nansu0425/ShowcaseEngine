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

Order parameters as follows:

1. Context / system objects first (`RenderContext&`, `CommandList&`)
2. Input parameters next
3. Output parameters last, prefixed with `out` (`Model& outModel`)

```cpp
static bool LoadGLTF(const std::string& filepath,
                     RenderContext& ctx,
                     Model& outModel);
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
| Expected failure | External (file missing, GPU error, bad `HRESULT`) | Log + `return false` | `SE_LOG_ERROR` |
| Programmer mistake | Internal (broken invariant, bad argument, impossible state) | Halt immediately in Debug | `SE_ASSERT` |

### Expected Failures

- Use the `Init()` / `Shutdown()` pattern. `Init()` returns `bool`.
- Use `SE_LOG_*` macros for diagnostics.
- No exceptions.

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
