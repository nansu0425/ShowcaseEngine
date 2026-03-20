# ShowcaseEngine

A game engine for demonstrating game tech concepts and principles in YouTube content — C++20 / DirectX 12 / ImGui / CMake

## Build

```batch
scripts/build.bat configure          # Generate CMake (Visual Studio 17 2022)
scripts/build.bat build Debug        # Debug build
scripts/build.bat build Release      # Release build
```

## Directory Structure

```
engine/          Engine core (STATIC lib, alias ShowcaseEngine::Core)
  include/showcase/{core,graphics,demo,ui}/   Public headers
  src/                                        Implementations
app/             ShowcaseApp executable (WinMain entry point)
showcases/       Individual showcase modules (each a STATIC lib)
cmake/           Dependencies.cmake, ShaderCompilation.cmake
```

## Adding a New Showcase

Follow the structure of `showcases/01_hello_triangle/` (h/cpp/CMakeLists.txt/shaders/).

- `REGISTER_SHOWCASE(ClassName)` macro required in cpp
- Must link with `$<LINK_LIBRARY:WHOLE_ARCHIVE,...>` in `app/CMakeLists.txt` — otherwise MSVC linker strips the static initializer and the showcase won't register
- Compile shaders via `target_compile_shaders()` (`cmake/ShaderCompilation.cmake`)
- Reuse precompiled headers via `target_precompile_headers(<target> REUSE_FROM showcase_engine)` in showcase CMakeLists.txt

## Shader Pipeline

- Output naming: `{NAME_WE}_{type}.cso` — e.g. `triangle_vs.hlsl` → `triangle_vs_vs.cso`
- Runtime loading: `ctx.GetShaderManager().LoadShader("shaders/triangle_vs_vs.cso")` — relative to exe directory

## Conventions

- **Language**: All documentation and code comments must be written in English
- **Namespace**: `showcase`
- **Includes**: Public headers `<showcase/module/File.h>`, internal headers `"File.h"`
- **GPU resource ownership**: `ComPtr<>` (COM), `D3D12MA::Allocation` (GPU memory), `std::unique_ptr<>` (general)

## Gotchas

- `ShowcaseRegistry::Register()` runs during static initialization — do not use `SE_LOG_*` (logger not yet initialized)
- Shader paths resolve relative to exe directory, not CWD (`GetExecutableDir()` in `ShaderManager.cpp`)
- SwapChain triple-buffering (BUFFER_COUNT=3), FrameResource also 3-frame buffering
