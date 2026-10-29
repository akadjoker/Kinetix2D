set(K2D_ZEN_SOURCES
    src/ZenDebugger.cpp
    src/ZenRuntime.cpp
    src/ZenScriptComponent.cpp
    src/ZenScriptProperties.cpp
)

add_library(k2d_zen STATIC ${K2D_ZEN_SOURCES})
target_include_directories(k2d_zen PUBLIC include)
target_link_libraries(k2d_zen PUBLIC k2d k2d_physics2d zen_static)
target_compile_features(k2d_zen PRIVATE cxx_std_17)

# The same scripting on zen_debug, for k2d_runner_debug alone.
if(TARGET zen_debug)
    add_library(k2d_zen_debug STATIC ${K2D_ZEN_SOURCES})
    target_include_directories(k2d_zen_debug PUBLIC include)
    target_link_libraries(k2d_zen_debug PUBLIC k2d k2d_physics2d zen_debug)
    target_compile_features(k2d_zen_debug PRIVATE cxx_std_17)
endif()
