add_library(showcase_engine STATIC
    # Core
    src/core/Log.cpp
    src/core/Platform.cpp
    src/core/Window.cpp
    src/core/Timer.cpp
    src/core/Input.cpp
    src/core/JsonDocument.cpp

    # Graphics
    src/graphics/Device.cpp
    src/graphics/CommandQueue.cpp
    src/graphics/DescriptorHeap.cpp
    src/graphics/SwapChain.cpp
    src/graphics/CommandList.cpp
    src/graphics/RootSignature.cpp
    src/graphics/PipelineState.cpp
    src/graphics/Buffer.cpp
    src/graphics/FrameResource.cpp
    src/graphics/RenderContext.cpp
    src/graphics/ShaderManager.cpp
    src/graphics/DepthBuffer.cpp
    src/graphics/Camera.cpp
    src/graphics/Texture.cpp
    src/graphics/Model.cpp
    src/graphics/Scene.cpp
    src/graphics/RenderTarget.cpp
    src/graphics/SceneRenderer.cpp
    src/graphics/AssetManager.cpp
    src/graphics/OffscreenTarget.cpp
)

add_library(ShowcaseEngine::Core ALIAS showcase_engine)

target_include_directories(showcase_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(showcase_engine
    PUBLIC
        d3d12
        dxgi
        dxguid
        d3dcompiler
        D3D12MemoryAllocator
        spdlog::spdlog
        directxtk12_headers
        tinygltf_headers
        ShowcaseEngine::Warnings
    PRIVATE
        nlohmann_json::nlohmann_json
)

target_compile_features(showcase_engine PUBLIC cxx_std_20)

# Precompiled headers
target_precompile_headers(showcase_engine PRIVATE
    # Windows / COM
    <wrl/client.h>

    # DirectX 12
    <d3d12.h>
    <dxgi1_6.h>
    <D3D12MemAlloc.h>

    # Standard Library
    <memory>
    <string>
    <vector>
    <stdexcept>
    <cstdint>
    <functional>

    # Third-party
    <spdlog/spdlog.h>
    <SimpleMath.h>
)

# Enable Unicode for Win32
target_compile_definitions(showcase_engine PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

# Compile shaders
target_compile_shaders(showcase_engine
    VERTEX ${CMAKE_CURRENT_SOURCE_DIR}/shaders/mesh_vs.hlsl
    PIXEL  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/mesh_ps.hlsl
)
