add_library(showcase_warnings INTERFACE)
target_compile_options(showcase_warnings INTERFACE
    /W4
    /permissive-
    /wd4100  # unreferenced formal parameter
    /wd4201  # nonstandard extension: nameless struct/union (DirectXMath)
)
add_library(ShowcaseEngine::Warnings ALIAS showcase_warnings)
