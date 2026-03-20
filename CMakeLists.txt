cmake_minimum_required(VERSION 3.24)
project(ShowcaseEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output binaries to a common directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Include utility modules
include(cmake/Dependencies.cmake)
include(cmake/CompilerWarnings.cmake)
include(cmake/ShaderCompilation.cmake)

# Engine static library
add_subdirectory(engine)

# Showcases
add_subdirectory(showcases)

# Application executable
add_subdirectory(app)
