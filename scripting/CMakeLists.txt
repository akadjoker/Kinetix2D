add_library(k2d_zen STATIC
    src/ZenScriptComponent.cpp
)

target_include_directories(k2d_zen PUBLIC include)
target_link_libraries(k2d_zen PUBLIC k2d zen_static)
