set(K2D_EDITOR_ICONS_PNG_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/k2d_editor_icons_png.h")
add_custom_command(
    OUTPUT "${K2D_EDITOR_ICONS_PNG_HEADER}"
    COMMAND ${CMAKE_COMMAND}
        -DINPUT="${CMAKE_CURRENT_SOURCE_DIR}/icons/atlas.png"
        -DOUTPUT="${K2D_EDITOR_ICONS_PNG_HEADER}"
        -DVARIABLE=kEditorIconsPng
        -P "${CMAKE_SOURCE_DIR}/cmake/EmbedBinary.cmake"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/icons/atlas.png" "${CMAKE_SOURCE_DIR}/cmake/EmbedBinary.cmake")
set(K2D_EDITOR_ICONS_TXT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/k2d_editor_icons_txt.h")
add_custom_command(
    OUTPUT "${K2D_EDITOR_ICONS_TXT_HEADER}"
    COMMAND ${CMAKE_COMMAND}
        -DINPUT="${CMAKE_CURRENT_SOURCE_DIR}/icons/atlas.txt"
        -DOUTPUT="${K2D_EDITOR_ICONS_TXT_HEADER}"
        -DVARIABLE=kEditorIconsTxt
        -P "${CMAKE_SOURCE_DIR}/cmake/EmbedBinary.cmake"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/icons/atlas.txt" "${CMAKE_SOURCE_DIR}/cmake/EmbedBinary.cmake")

add_executable(k2d_editor
    src/main.cpp
    src/Icons.cpp
    "${K2D_EDITOR_ICONS_PNG_HEADER}"
    "${K2D_EDITOR_ICONS_TXT_HEADER}"
    src/EditorWindow.cpp
    src/HierarchyPanel.cpp
    src/InspectorPanel.cpp
    src/ViewportPanel.cpp
    src/SceneGizmo.cpp
    src/SceneShapes.cpp
    src/AssetsPanel.cpp
    src/SpriteCanvas.cpp
    src/SpriteEditorPanel.cpp
    src/FrameStrip.cpp
    src/AnimatorPanel.cpp
    src/TilePalette.cpp
    src/TilePainterPanel.cpp
    src/ScriptEditorPanel.cpp
    src/ParticlesPanel.cpp
    src/McpServer.cpp
    src/SettingsPanel.cpp
    src/ProfilerPanel.cpp
    src/ImageEditorPanel.cpp
    src/PropertyFields.cpp
    src/core/EditorFileSystem.cpp
    src/core/CommandStack.cpp
)
target_include_directories(k2d_editor PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(k2d_editor PRIVATE k2d k2d_zen k2d_physics2d igui::zen)
target_compile_features(k2d_editor PRIVATE cxx_std_17)
if(NOT MSVC)
    target_compile_options(k2d_editor PRIVATE -fno-rtti)
endif()
set_target_properties(k2d_editor PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${K2D_RUNTIME_OUTPUT_DIRECTORY})
