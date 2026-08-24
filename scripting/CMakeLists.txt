add_library(k2d_zen STATIC
    src/ZenRuntime.cpp
    src/ZenScriptComponent.cpp
    src/ZenScriptProperties.cpp
)

target_include_directories(k2d_zen PUBLIC include)
target_link_libraries(k2d_zen PUBLIC k2d k2d_physics2d zen_static)
target_compile_features(k2d_zen PRIVATE cxx_std_17)
