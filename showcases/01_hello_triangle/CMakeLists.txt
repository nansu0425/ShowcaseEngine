add_library(showcase_hello_triangle STATIC
    HelloTriangleShowcase.cpp
)

target_link_libraries(showcase_hello_triangle PUBLIC ShowcaseEngine::Core)

target_precompile_headers(showcase_hello_triangle REUSE_FROM showcase_engine)

target_include_directories(showcase_hello_triangle PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Compile shaders
target_compile_shaders(showcase_hello_triangle
    VERTEX ${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle_vs.hlsl
    PIXEL  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle_ps.hlsl
)
