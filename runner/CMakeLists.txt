add_executable(k2d_runner
    src/main.cpp
)

target_link_libraries(k2d_runner PRIVATE k2d k2d_zen k2d_physics2d)
target_compile_features(k2d_runner PRIVATE cxx_std_17)

set_target_properties(k2d_runner PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${K2D_RUNTIME_OUTPUT_DIRECTORY}
)
