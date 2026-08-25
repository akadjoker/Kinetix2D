cmake_minimum_required(VERSION 3.16)

find_package(SDL2 REQUIRED)
if(ANDROID)
    # zlib is part of the Android NDK system libraries.
    set(K2D_ZLIB_LIBRARY z)
elseif(EMSCRIPTEN)
    # Pull zlib from Emscripten's port instead of looking for a host library.
    set(K2D_ZLIB_LINK_OPTION "-sUSE_ZLIB=1")
else()
    find_package(ZLIB REQUIRED)
    set(K2D_ZLIB_LIBRARY ZLIB::ZLIB)
endif()

# WebGL 2 and Android already provide the GLES3 entry points. Desktop still
# needs glad to resolve them from SDL's GL context at runtime.
set(K2D_NATIVE_GLES3 OFF)
if(ANDROID OR EMSCRIPTEN)
    set(K2D_NATIVE_GLES3 ON)
else()
    add_library(glad STATIC ${CMAKE_SOURCE_DIR}/external/glad/src/glad.cpp)
    target_include_directories(glad PUBLIC ${CMAKE_SOURCE_DIR}/external/glad/include)
endif()

add_library(imgui STATIC
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui_stdlib.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/backends/imgui_impl_sdl2.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    ${CMAKE_SOURCE_DIR}/external/imgui
    ${CMAKE_SOURCE_DIR}/external/imgui/backends
)

if(K2D_NATIVE_GLES3)
    target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_ES3)
    set(K2D_IMGUI_GL_INCLUDE "GLES3/gl3.h")
else()
    target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_CUSTOM)
    target_link_libraries(imgui PUBLIC glad)
    set(K2D_IMGUI_GL_INCLUDE "${CMAKE_SOURCE_DIR}/external/glad/include/glad/glad.h")
endif()

set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/external/imgui/backends/imgui_impl_opengl3.cpp
    PROPERTIES
    COMPILE_OPTIONS "-include${K2D_IMGUI_GL_INCLUDE}"
)

# The default UI skin is owned by the engine, not by a game project. Embed it
# so a missing external theme never leaves the game with invisible controls.
set(K2D_DEFAULT_UI_THEME_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/k2d_default_ui_theme_png.h")
add_custom_command(
    OUTPUT "${K2D_DEFAULT_UI_THEME_HEADER}"
    COMMAND ${CMAKE_COMMAND}
        -DINPUT="${CMAKE_SOURCE_DIR}/assets/menu.png"
        -DOUTPUT="${K2D_DEFAULT_UI_THEME_HEADER}"
        -P "${CMAKE_SOURCE_DIR}/cmake/EmbedBinary.cmake"
    DEPENDS
        "${CMAKE_SOURCE_DIR}/assets/menu.png"
        "${CMAKE_SOURCE_DIR}/cmake/EmbedBinary.cmake"
    COMMENT "Embedding Kinetix2D default UI theme"
)
add_custom_target(k2d_default_ui_theme_asset DEPENDS "${K2D_DEFAULT_UI_THEME_HEADER}")

# The compact virtual-pad atlas contains only the four circular controls used
# at runtime, keeping the decoded texture at 512x512 instead of 1024x1024.
set(K2D_DEFAULT_VIRTUAL_PAD_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/k2d_default_virtual_pad_png.h")
add_custom_command(
    OUTPUT "${K2D_DEFAULT_VIRTUAL_PAD_HEADER}"
    COMMAND ${CMAKE_COMMAND}
        -DINPUT="${CMAKE_SOURCE_DIR}/assets/gamepad_circles.png"
        -DOUTPUT="${K2D_DEFAULT_VIRTUAL_PAD_HEADER}"
        -DVARIABLE=kDefaultVirtualPadPng
        -P "${CMAKE_SOURCE_DIR}/cmake/EmbedBinary.cmake"
    DEPENDS
        "${CMAKE_SOURCE_DIR}/assets/gamepad_circles.png"
        "${CMAKE_SOURCE_DIR}/cmake/EmbedBinary.cmake"
    COMMENT "Embedding Kinetix2D default virtual pad"
)
add_custom_target(k2d_default_virtual_pad_asset DEPENDS "${K2D_DEFAULT_VIRTUAL_PAD_HEADER}")

# miniaudio is compiled in exactly one C translation unit. Keeping it as C is
# required for the Emscripten Web Audio callbacks to retain their C symbols.
add_library(k2d_miniaudio STATIC src/audio/miniaudio_impl.c)
target_include_directories(k2d_miniaudio PRIVATE src/audio)
set_source_files_properties(src/audio/miniaudio_impl.c PROPERTIES LANGUAGE C COMPILE_OPTIONS "-w")
if(UNIX AND NOT APPLE)
    target_link_libraries(k2d_miniaudio PUBLIC pthread dl m)
endif()
if(APPLE)
    target_link_libraries(k2d_miniaudio PUBLIC
        "-framework CoreFoundation" "-framework CoreAudio" "-framework AudioToolbox" "-framework AudioUnit")
endif()
if(ANDROID)
    target_link_libraries(k2d_miniaudio PUBLIC aaudio OpenSLES log)
endif()

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
    src/InputActionMap.cpp
    src/AudioEngine.cpp
    src/AudioPlayer.cpp
    src/KPak.cpp
    src/FileBuffer.cpp
    src/FileSystem.cpp
    src/UserData.cpp
    src/Device.cpp
    src/Shader.cpp
    src/Texture.cpp
    src/UiTheme.cpp
    src/UiControls.cpp
    src/Pixmap.cpp
    src/Assets.cpp
    src/Batch.cpp
    src/CameraComponent.cpp
    src/Animation2D.cpp
    src/Material2D.cpp
    src/CanvasRenderer.cpp
    src/RenderState.cpp
    src/RenderQueue.cpp
    src/Component.cpp
    src/GameObject.cpp
    src/Light2D.cpp
    src/DirectionalLight2D.cpp
    src/ParticleSystem.cpp
    src/ParticleComponent.cpp
    src/LightOccluder2D.cpp
    src/Scene.cpp
    src/SceneManager.cpp
    src/SpriteComponent.cpp
    src/SpriteBatch.cpp
    src/SpriteAtlas.cpp
    src/Polygon2D.cpp
    src/Line2D.cpp
    src/CircleShape.cpp
    src/RectShape.cpp
    src/CapsuleShape.cpp
    src/NinePatchComponent.cpp
    src/ScreenFade.cpp
    src/VirtualPad.cpp
    src/SceneDebug.cpp
    src/CanvasLayer.cpp
    src/AStarGrid2D.cpp
    src/AStar2D.cpp
    src/Navigation2D.cpp
    src/NavigationRegion2D.cpp
    src/NavigationAgent2D.cpp
    src/MotionTween2D.cpp
    src/MotionStreak2D.cpp
    src/Serializer.cpp
    src/Prefab.cpp
    ${CMAKE_SOURCE_DIR}/physics/src/triangulate.cpp
    src/TileMapComponent.cpp
    src/ParallaxLayer2D.cpp
    src/Profiler.cpp
    src/ProfilerUI.cpp
)

target_include_directories(k2d PUBLIC
    include
    ${CMAKE_SOURCE_DIR}/external/stb
    ${CMAKE_SOURCE_DIR}/physics/include
    ${CMAKE_SOURCE_DIR}/external/containers/include
)

if(NOT K2D_NATIVE_GLES3)
    target_include_directories(k2d PUBLIC ${CMAKE_SOURCE_DIR}/external/glad/include)
endif()

target_include_directories(k2d PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_dependencies(k2d k2d_default_ui_theme_asset k2d_default_virtual_pad_asset)

target_link_libraries(k2d PUBLIC mathc k2d_miniaudio ${K2D_ZLIB_LIBRARY})
if(EMSCRIPTEN)
    target_link_options(k2d PUBLIC ${K2D_ZLIB_LINK_OPTION})
endif()
if(K2D_NATIVE_GLES3)
    if(ANDROID)
        target_link_libraries(k2d PUBLIC GLESv3)
    endif()
    if(EMSCRIPTEN)
        target_link_options(k2d PUBLIC "-sMIN_WEBGL_VERSION=2" "-sMAX_WEBGL_VERSION=2")
    endif()
else()
    target_link_libraries(k2d PUBLIC glad)
endif()
target_link_libraries(k2d PRIVATE imgui)

if(TARGET SDL2::SDL2)
    target_link_libraries(k2d PUBLIC SDL2::SDL2)
else()
    target_include_directories(k2d PUBLIC ${SDL2_INCLUDE_DIRS})
    target_link_libraries(k2d PUBLIC ${SDL2_LIBRARIES})
endif()

target_precompile_headers(k2d PRIVATE src/pch.h)
