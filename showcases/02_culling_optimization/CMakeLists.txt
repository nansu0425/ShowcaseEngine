add_library(showcase_culling_optimization STATIC
    CullingShowcase.cpp
    FPSCameraController.cpp
)

target_link_libraries(showcase_culling_optimization PUBLIC ShowcaseEngine::Core)

target_precompile_headers(showcase_culling_optimization REUSE_FROM showcase_engine)

target_include_directories(showcase_culling_optimization PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Compile shaders
target_compile_shaders(showcase_culling_optimization
    VERTEX ${CMAKE_CURRENT_SOURCE_DIR}/shaders/mesh_vs.hlsl
    PIXEL  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/mesh_ps.hlsl
)
