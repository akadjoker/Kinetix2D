add_executable(k2d_editor
    src/main.cpp
    src/EditorWindow.cpp
    src/HierarchyPanel.cpp
    src/InspectorPanel.cpp
    src/ViewportPanel.cpp
)
target_include_directories(k2d_editor PRIVATE src)
target_link_libraries(k2d_editor PRIVATE k2d k2d_zen k2d_physics2d igui::zen)
target_compile_features(k2d_editor PRIVATE cxx_std_17)
if(NOT MSVC)
    target_compile_options(k2d_editor PRIVATE -fno-rtti)
endif()
set_target_properties(k2d_editor PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${K2D_RUNTIME_OUTPUT_DIRECTORY})
