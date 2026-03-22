# Validates that editor source files do not directly include platform or
# engine-only library headers.  Runs as a PRE_BUILD step so violations
# block compilation with a clear error message.
#
# Usage (in editor/CMakeLists.txt):
#   target_check_editor_includes(ShowcaseEditor)

function(target_check_editor_includes TARGET)
    find_program(POWERSHELL_EXECUTABLE powershell)
    if(NOT POWERSHELL_EXECUTABLE)
        message(WARNING "PowerShell not found. Editor include guard disabled.")
        return()
    endif()

    add_custom_command(TARGET ${TARGET} PRE_BUILD
        COMMAND ${POWERSHELL_EXECUTABLE} -ExecutionPolicy Bypass -File
            ${CMAKE_SOURCE_DIR}/scripts/check_editor_includes.ps1
            -EditorDir ${CMAKE_SOURCE_DIR}/editor
        COMMENT "Checking editor include policy..."
        VERBATIM
    )
endfunction()
