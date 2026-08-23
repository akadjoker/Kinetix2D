add_library(k2d_editor_widgets STATIC
    external/imgui_widgets/ImGuiFileDialog.cpp
    external/imgui_widgets/ImGuiConsole.cpp
    external/imgui_widgets/ImCurveEdit.cpp
    external/imgui_widgets/ImGradient.cpp
    external/imgui_widgets/ImSequencer.cpp
)

target_include_directories(k2d_editor_widgets PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui_widgets
)

target_link_libraries(k2d_editor_widgets PUBLIC imgui)
target_compile_features(k2d_editor_widgets PUBLIC cxx_std_17)

add_executable(k2d_editor
    src/main.cpp
    src/core/EditorApplication.cpp
    src/core/CommandStack.cpp
    src/core/EditorFileSystem.cpp
    src/core/EditorSelection.cpp
    src/core/EditorSettings.cpp
    src/core/EditorToasts.cpp
    src/core/Project.cpp
    src/panels/AssetsPanel.cpp
    src/panels/ConsolePanel.cpp
    src/panels/GamePanel.cpp
    src/panels/HierarchyPanel.cpp
    src/panels/InspectorPanel.cpp
    src/panels/ParticlePanel.cpp
    src/panels/PrefabsPanel.cpp
    src/panels/SceneViewportPanel.cpp
)

target_include_directories(k2d_editor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/external/icons
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/panels
    ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets
)

target_link_libraries(k2d_editor PRIVATE k2d k2d_zen k2d_editor_widgets imgui)
target_compile_features(k2d_editor PRIVATE cxx_std_17)

set_target_properties(k2d_editor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)
