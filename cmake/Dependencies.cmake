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

# DirectXTK12 (SimpleMath, SpriteBatch, etc.)
FetchContent_Declare(
    directxtk12
    GIT_REPOSITORY https://github.com/microsoft/DirectXTK12.git
    GIT_TAG        oct2025
    GIT_SHALLOW    TRUE
    EXCLUDE_FROM_ALL
)

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

# tinygltf (header-only glTF loader)
FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG        v2.9.5
    GIT_SHALLOW    TRUE
    EXCLUDE_FROM_ALL
)

# Suppress deprecation warnings from third-party CMakeLists.txt
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

# Make available: D3D12MA, spdlog, json
FetchContent_MakeAvailable(D3D12MemoryAllocator spdlog nlohmann_json)

# DirectXTK12 — fetch source only (SimpleMath is header-only)
FetchContent_MakeAvailable(directxtk12)
add_library(directxtk12_headers INTERFACE)
target_include_directories(directxtk12_headers INTERFACE ${directxtk12_SOURCE_DIR}/Inc)

# tinygltf — fetch source only (header-only library)
FetchContent_MakeAvailable(tinygltf)
add_library(tinygltf_headers INTERFACE)
target_include_directories(tinygltf_headers INTERFACE ${tinygltf_SOURCE_DIR})
target_compile_definitions(tinygltf_headers INTERFACE TINYGLTF_NO_INCLUDE_JSON)

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

# ImGuizmo (transform gizmo overlay for ImGui)
FetchContent_Declare(
    imguizmo
    GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imguizmo)
add_library(imguizmo_lib STATIC ${imguizmo_SOURCE_DIR}/ImGuizmo.cpp)
target_include_directories(imguizmo_lib PUBLIC ${imguizmo_SOURCE_DIR})
target_link_libraries(imguizmo_lib PUBLIC imgui::imgui)

set(CMAKE_WARN_DEPRECATED ON CACHE BOOL "" FORCE)
