add_library(showcase_engine STATIC
    # Core
    src/core/Log.cpp
    src/core/Window.cpp
    src/core/Timer.cpp
    src/core/Input.cpp
    src/core/Application.cpp

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

    # UI
    src/ui/ImGuiLayer.cpp
    src/ui/OverlaySystem.cpp

    # Demo
    src/demo/ShowcaseRegistry.cpp
    src/demo/ShowcaseManager.cpp
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
        imgui::imgui
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        ShowcaseEngine::Warnings
)

target_compile_features(showcase_engine PUBLIC cxx_std_20)

# Enable Unicode for Win32
target_compile_definitions(showcase_engine PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
