# ShowcaseEngine

A game engine for demonstrating game tech concepts and principles in YouTube content — C++20 / DirectX 12 / ImGui / CMake

## Target Audience

The YouTube content targets **non-developers** — viewers who have never written code. Explanations should prioritize visual clarity and intuitive understanding over technical depth. Think "revealing the hidden tricks behind what gamers already experience" rather than "teaching graphics programming."

## Build

```batch
scripts/build.bat configure          # Generate CMake (Visual Studio 17 2022)
scripts/build.bat build Debug        # Debug build
scripts/build.bat build Release      # Release build
```

## Directory Structure

```
engine/          Engine core (STATIC lib, alias ShowcaseEngine::Core — no ImGui dependency)
  include/showcase/{core,graphics}/      Public headers
  src/                                   Implementations
  shaders/                               HLSL shaders
editor/          ShowcaseEditor executable (links engine + ImGui/ImGuizmo)
  include/showcase/editor/              Editor headers (EditorApp, ViewportPanel, Console, etc.)
  src/                                  Editor implementations + WinMain entry point
docs/            Planning documents (content plans, tech specs, progress)
  ARCHITECTURE.md                       Engine & editor architecture overview — refer to this for structural context
cmake/           Dependencies.cmake, ShaderCompilation.cmake
```

## Shader Pipeline

- Output naming: `{NAME_WE}_{type}.cso` — e.g. `mesh_vs.hlsl` → `mesh_vs_vs.cso`
- Runtime loading: `ctx.GetShaderManager().LoadShader("shaders/mesh_vs_vs.cso")` — relative to exe directory
- Compile shaders via `target_compile_shaders()` (`cmake/ShaderCompilation.cmake`)

## Conventions

- **Language**: All documentation and code comments must be written in English
- **Namespace**: `showcase`
- **Includes**: Public headers `<showcase/module/File.h>`, internal headers `"File.h"`
- **GPU resource ownership**: `ComPtr<>` (COM), `D3D12MA::Allocation` (GPU memory), `std::unique_ptr<>` (general)
- **Coding convention**: See `docs/CODING_CONVENTION.md` for full naming, formatting, and style rules
- **Architecture doc sync**: When making architectural changes (adding/removing/renaming modules, classes, or dependencies; changing rendering pipeline, resource ownership, or build structure), update `docs/ARCHITECTURE.md` to reflect the current state

## Gotchas

- Shader paths resolve relative to exe directory, not CWD (`GetExecutableDir()` in `ShaderManager.cpp`)
- SwapChain triple-buffering (BUFFER_COUNT=3), FrameResource also 3-frame buffering
- HLSL `cbuffer` matrices default to column-major, but SimpleMath stores row-major — always declare `row_major float4x4` in HLSL, or transpose on CPU before upload
- A single upload-heap constant buffer rewritten per draw call causes GPU aliasing (flickering) — use offset-based writes (`UpdateDataAtOffset`) so each object occupies its own 256-byte-aligned slot
- SimpleMath `Matrix::CreateLookAt` / `CreatePerspectiveFieldOfView` produce RH matrices — use `XMMatrixLookAtLH` / `XMMatrixPerspectiveFovLH` directly for LH pipelines
- Incremental Debug builds can fail with `LNK1103: debugging information corrupt` after large header changes (struct/include modifications) — stale PCH objects cause PDB mismatch. Fix: `scripts/build.bat nuke` then reconfigure and rebuild
