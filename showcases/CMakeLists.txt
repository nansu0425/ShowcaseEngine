# Auto-discover showcase subdirectories
file(GLOB showcase_dirs RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *)
foreach(dir ${showcase_dirs})
    if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${dir} AND
       EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/CMakeLists.txt)
        add_subdirectory(${dir})
    endif()
endforeach()
