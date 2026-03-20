# Find DXC shader compiler
find_program(DXC_EXECUTABLE dxc
    HINTS
        "$ENV{WindowsSdkDir}/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64"
)

if(NOT DXC_EXECUTABLE)
    message(WARNING "DXC shader compiler not found. Shaders will not be compiled.")
endif()

# Function to compile HLSL shaders
# Usage: target_compile_shaders(TARGET
#            VERTEX shader1.hlsl shader2.hlsl
#            PIXEL shader3.hlsl shader4.hlsl
#            COMPUTE shader5.hlsl)
function(target_compile_shaders TARGET)
    if(NOT DXC_EXECUTABLE)
        return()
    endif()

    cmake_parse_arguments(SHADER "" "OUTPUT_DIR" "VERTEX;PIXEL;COMPUTE" ${ARGN})

    if(NOT SHADER_OUTPUT_DIR)
        set(SHADER_OUTPUT_DIR ${CMAKE_BINARY_DIR}/shaders)
    endif()

    file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

    # Compile vertex shaders
    foreach(shader_file ${SHADER_VERTEX})
        get_filename_component(shader_name ${shader_file} NAME_WE)
        set(output_file ${SHADER_OUTPUT_DIR}/${shader_name}_vs.cso)
        add_custom_command(
            OUTPUT ${output_file}
            COMMAND ${DXC_EXECUTABLE} -T vs_6_0 -E main -Fo ${output_file} -Zi ${shader_file}
            DEPENDS ${shader_file}
            COMMENT "Compiling vertex shader: ${shader_name}"
            VERBATIM
        )
        target_sources(${TARGET} PRIVATE ${output_file})
    endforeach()

    # Compile pixel shaders
    foreach(shader_file ${SHADER_PIXEL})
        get_filename_component(shader_name ${shader_file} NAME_WE)
        set(output_file ${SHADER_OUTPUT_DIR}/${shader_name}_ps.cso)
        add_custom_command(
            OUTPUT ${output_file}
            COMMAND ${DXC_EXECUTABLE} -T ps_6_0 -E main -Fo ${output_file} -Zi ${shader_file}
            DEPENDS ${shader_file}
            COMMENT "Compiling pixel shader: ${shader_name}"
            VERBATIM
        )
        target_sources(${TARGET} PRIVATE ${output_file})
    endforeach()

    # Compile compute shaders
    foreach(shader_file ${SHADER_COMPUTE})
        get_filename_component(shader_name ${shader_file} NAME_WE)
        set(output_file ${SHADER_OUTPUT_DIR}/${shader_name}_cs.cso)
        add_custom_command(
            OUTPUT ${output_file}
            COMMAND ${DXC_EXECUTABLE} -T cs_6_0 -E main -Fo ${output_file} -Zi ${shader_file}
            DEPENDS ${shader_file}
            COMMENT "Compiling compute shader: ${shader_name}"
            VERBATIM
        )
        target_sources(${TARGET} PRIVATE ${output_file})
    endforeach()
endfunction()
