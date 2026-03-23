add_executable(ShowcaseEditor WIN32
    src/Main.cpp
    src/EditorApp.cpp
    src/ImGuiLayer.cpp
    src/Console.cpp
    src/ViewportPanel.cpp
    src/EditorController.cpp
)

target_include_directories(ShowcaseEditor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ShowcaseEditor PRIVATE
    ShowcaseEngine::Core
    imgui::imgui
    imguizmo_lib
)

target_precompile_headers(ShowcaseEditor PRIVATE
    <imgui.h>
)

# Enforce engine/editor dependency boundary
target_check_editor_includes(ShowcaseEditor)

# Copy shaders to output directory
add_custom_command(TARGET ShowcaseEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_BINARY_DIR}/shaders
        $<TARGET_FILE_DIR:ShowcaseEditor>/shaders
    COMMENT "Copying compiled shaders to output directory"
)

# Copy assets to output directory
add_custom_command(TARGET ShowcaseEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:ShowcaseEditor>/assets
    COMMENT "Copying assets to output directory"
)
