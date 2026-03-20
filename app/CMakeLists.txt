add_executable(ShowcaseApp WIN32 main.cpp)

target_link_libraries(ShowcaseApp PRIVATE
    ShowcaseEngine::Core
    $<LINK_LIBRARY:WHOLE_ARCHIVE,showcase_hello_triangle>
)

target_precompile_headers(ShowcaseApp REUSE_FROM showcase_engine)

# Copy shaders to output directory
add_custom_command(TARGET ShowcaseApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_BINARY_DIR}/shaders
        $<TARGET_FILE_DIR:ShowcaseApp>/shaders
    COMMENT "Copying compiled shaders to output directory"
)
