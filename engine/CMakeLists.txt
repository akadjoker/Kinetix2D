cmake_minimum_required(VERSION 3.16)

set(K2D_NATIVE_GLES3 OFF)
if(ANDROID OR EMSCRIPTEN)
    set(K2D_NATIVE_GLES3 ON)
endif()

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
if(UNIX AND NOT APPLE AND NOT ANDROID)
    target_link_libraries(k2d_miniaudio PUBLIC pthread dl m)
endif()
if(APPLE)
    target_link_libraries(k2d_miniaudio PUBLIC
        "-framework CoreFoundation" "-framework CoreAudio" "-framework AudioToolbox" "-framework AudioUnit")
endif()
if(ANDROID)
    target_link_libraries(k2d_miniaudio PUBLIC aaudio OpenSLES log)
endif()

add_library(k2d STATIC
    src/Input.cpp
    src/InputActionMap.cpp
    src/AudioEngine.cpp
    src/AudioPlayer.cpp
    src/KPak.cpp
    src/FileBuffer.cpp
    src/RenderTarget.cpp
    src/Image.cpp
    src/PropertyValue.cpp
    src/ClassDB.cpp
    src/ClassDBCore.cpp
    src/FileSystem.cpp
    src/UserData.cpp
    src/Device.cpp
    src/OpenGL.cpp
    src/Compress.cpp
    src/Shader.cpp
    src/Texture.cpp
    src/UiTheme.cpp
    src/UiControls.cpp
    src/Pixmap.cpp
    src/Assets.cpp
    src/Batch.cpp
    src/CameraComponent.cpp
    src/Animation2D.cpp
    src/Bone2D.cpp
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
    src/ParticlePhysics2D.cpp
    src/LightOccluder2D.cpp
    src/Scene.cpp
    src/SceneManager.cpp
    src/Skeleton2D.cpp
    src/SpriteComponent.cpp
    src/SpriteBatch.cpp
    src/SpriteAtlas.cpp
    src/Polygon2D.cpp
    src/Line2D.cpp
    src/CircleShape.cpp
    src/RectShape.cpp
    src/CapsuleShape.cpp
    src/NinePatchComponent.cpp
    src/MouseCursor.cpp
    src/ScreenFade.cpp
    src/TileSet.cpp
    src/CollisionLayers.cpp
    src/SceneTransition.cpp
    src/VirtualPad.cpp
    src/SceneDebug.cpp
    src/CanvasLayer.cpp
    src/AStarGrid2D.cpp
    src/AStar2D.cpp
    src/Navigation2D.cpp
    src/NavigationRegion2D.cpp
    src/NavigationAgent2D.cpp
    src/Formation2D.cpp
    src/Easing.cpp
    src/MotionTween2D.cpp
    src/MotionStreak2D.cpp
    src/PathMotion2D.cpp
    src/ActionSequence2D.cpp
    src/Serializer.cpp
    src/Prefab.cpp
    src/Triangulate2D.cpp
    src/TileMapComponent.cpp
    src/ParallaxLayer2D.cpp
    src/ParallaxLayerComponent.cpp
    src/Profiler.cpp
    src/Geometry2D.cpp
    src/MaskContour2D.cpp
    src/Manifold2D.cpp
    src/Collide2D.cpp
    src/RayCast2D.cpp
    src/ShapeCast2D.cpp
    src/DynamicTree2D.cpp
    src/SceneBodies.cpp
    src/SceneSolver.cpp
    src/SceneQueries.cpp
    ${CMAKE_SOURCE_DIR}/physics2d/src/BoxCollider2D.cpp
    ${CMAKE_SOURCE_DIR}/physics2d/src/ChainCollider2D.cpp
    ${CMAKE_SOURCE_DIR}/physics2d/src/CharacterBody2D.cpp
    ${CMAKE_SOURCE_DIR}/physics2d/src/CircleCollider2D.cpp
    ${CMAKE_SOURCE_DIR}/physics2d/src/Collider2D.cpp
    ${CMAKE_SOURCE_DIR}/physics2d/src/DistanceJoint2D.cpp
    ${CMAKE_SOURCE_DIR}/physics2d/src/EdgeCollider2D.cpp
    ${CMAKE_SOURCE_DIR}/physics2d/src/GearJoint2D.cpp
    ${CMAKE_SOURCE_DIR}/physics2d/src/ImageShape2D.cpp
    ${CMAKE_SOURCE_DIR}/physics2d/src/Joint2D.cpp
    ${CMAKE_SOURCE_DIR}/physics2d/src/MotorJoint2D.cpp
    ${CMAKE_SOURCE_DIR}/physics2d/src/MouseJoint2D.cpp
    ${CMAKE_SOURCE_DIR}/physics2d/src/Physics2DSerializer.cpp
    ${CMAKE_SOURCE_DIR}/physics2d/src/PolygonCollider2D.cpp
    ${CMAKE_SOURCE_DIR}/physics2d/src/RevoluteJoint2D.cpp
    ${CMAKE_SOURCE_DIR}/physics2d/src/RigidBody2D.cpp
    ${CMAKE_SOURCE_DIR}/physics2d/src/Steering2D.cpp
    ${CMAKE_SOURCE_DIR}/physics2d/src/TileMapCollider2D.cpp
    ${CMAKE_SOURCE_DIR}/physics2d/src/WheelJoint2D.cpp
)

target_include_directories(k2d PUBLIC
    include
    ${CMAKE_SOURCE_DIR}/external/stb
    ${CMAKE_SOURCE_DIR}/physics2d/include
)
target_link_libraries(k2d PUBLIC ct)


target_include_directories(k2d PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_custom_target(k2d_check_no_stl
    COMMAND ${CMAKE_COMMAND} -DK2D_SOURCE_DIR=${CMAKE_SOURCE_DIR} -P ${CMAKE_SOURCE_DIR}/cmake/CheckNoStl.cmake
    COMMENT "Checking the sources for STL containers")
add_dependencies(k2d k2d_default_ui_theme_asset k2d_default_virtual_pad_asset k2d_check_no_stl)

target_link_libraries(k2d PUBLIC mathc k2d_miniaudio)
target_link_libraries(k2d PRIVATE miniz)
if(K2D_NATIVE_GLES3)
    if(ANDROID)
        target_link_libraries(k2d PUBLIC GLESv3)
    endif()
    if(EMSCRIPTEN)
        target_link_options(k2d PUBLIC "-sMIN_WEBGL_VERSION=2" "-sMAX_WEBGL_VERSION=2")
    endif()
else()
    target_link_libraries(k2d PUBLIC opengles3)
endif()
target_link_libraries(k2d PUBLIC platform)


target_precompile_headers(k2d PRIVATE src/pch.h)
