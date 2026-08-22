cmake_minimum_required(VERSION 3.16)

find_package(SDL2 REQUIRED)

add_library(glad STATIC ${CMAKE_SOURCE_DIR}/external/glad/src/glad.cpp)
target_include_directories(glad PUBLIC ${CMAKE_SOURCE_DIR}/external/glad/include)

add_library(imgui STATIC
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/backends/imgui_impl_sdl2.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    ${CMAKE_SOURCE_DIR}/external/imgui
    ${CMAKE_SOURCE_DIR}/external/imgui/backends
)

target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_CUSTOM)
target_link_libraries(imgui PUBLIC glad)

set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/external/imgui/backends/imgui_impl_opengl3.cpp
    PROPERTIES
    COMPILE_OPTIONS "-include${CMAKE_SOURCE_DIR}/external/glad/include/glad/glad.h"
)

if(TARGET SDL2::SDL2)
    target_link_libraries(imgui PUBLIC SDL2::SDL2)
else()
    target_include_directories(imgui PUBLIC ${SDL2_INCLUDE_DIRS})
    target_link_libraries(imgui PUBLIC ${SDL2_LIBRARIES})
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(imgui PRIVATE -w)
endif()

add_library(k2d STATIC
    src/Input.cpp
    src/FileBuffer.cpp
    src/FileSystem.cpp
    src/Device.cpp
    src/Shader.cpp
    src/Texture.cpp
    src/Pixmap.cpp
    src/Assets.cpp
    src/Batch.cpp
    src/CameraComponent.cpp
    src/Animation2D.cpp
    src/Material2D.cpp
    src/CanvasRenderer.cpp
    src/RenderQueue.cpp
    src/Component.cpp
    src/GameObject.cpp
    src/Light2D.cpp
    src/DirectionalLight2D.cpp
    src/ParticleSystem.cpp
    src/LightOccluder2D.cpp
    src/Scene.cpp
    src/SpriteComponent.cpp
    src/SpriteBatch.cpp
    src/Polygon2D.cpp
    ${CMAKE_SOURCE_DIR}/physics/src/triangulate.cpp
    src/TileMapComponent.cpp
    src/ParallaxLayer2D.cpp
    src/Profiler.cpp
    src/ProfilerUI.cpp
)

target_include_directories(k2d PUBLIC
    include
    ${CMAKE_SOURCE_DIR}/external/glad/include
    ${CMAKE_SOURCE_DIR}/external/stb
    ${CMAKE_SOURCE_DIR}/physics/include
    ${CMAKE_SOURCE_DIR}/external/containers/include
)

target_include_directories(k2d PRIVATE src)

target_link_libraries(k2d PUBLIC glad glm::glm)
target_link_libraries(k2d PRIVATE imgui)

if(TARGET SDL2::SDL2)
    target_link_libraries(k2d PUBLIC SDL2::SDL2)
else()
    target_include_directories(k2d PUBLIC ${SDL2_INCLUDE_DIRS})
    target_link_libraries(k2d PUBLIC ${SDL2_LIBRARIES})
endif()

target_precompile_headers(k2d PRIVATE src/pch.h)
