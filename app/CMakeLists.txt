add_executable(ShowcaseApp WIN32 main.cpp)

target_link_libraries(ShowcaseApp PRIVATE
    ShowcaseEngine::Core
)

target_precompile_headers(ShowcaseApp REUSE_FROM showcase_engine)

# Copy shaders to output directory
add_custom_command(TARGET ShowcaseApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_BINARY_DIR}/shaders
        $<TARGET_FILE_DIR:ShowcaseApp>/shaders
    COMMENT "Copying compiled shaders to output directory"
)

# Copy assets to output directory
add_custom_command(TARGET ShowcaseApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:ShowcaseApp>/assets
    COMMENT "Copying assets to output directory"
)
