include(FetchContent)

set(FETCHCONTENT_QUIET OFF)

# D3D12 Memory Allocator
FetchContent_Declare(
    D3D12MemoryAllocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator.git
    GIT_TAG        v2.0.1
    GIT_SHALLOW    TRUE
)

# Dear ImGui (docking branch)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.8-docking
    GIT_SHALLOW    TRUE
)

# DirectXTK12 — deferred to Milestone 2 (texture loading, SimpleMath)
# FetchContent_Declare(
#     directxtk12
#     GIT_REPOSITORY https://github.com/microsoft/DirectXTK12.git
#     GIT_TAG        oct2025
#     GIT_SHALLOW    TRUE
# )

# spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.15.0
    GIT_SHALLOW    TRUE
)

# nlohmann/json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)

# Make available: D3D12MA, spdlog, json
FetchContent_MakeAvailable(D3D12MemoryAllocator spdlog nlohmann_json)

# D3D12MA doesn't set its include directory, add it manually
target_include_directories(D3D12MemoryAllocator PUBLIC
    ${d3d12memoryallocator_SOURCE_DIR}/include
)

# ImGui requires manual target creation (no official CMakeLists.txt)
FetchContent_MakeAvailable(imgui)
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC d3d12 dxgi)
add_library(imgui::imgui ALIAS imgui_lib)
