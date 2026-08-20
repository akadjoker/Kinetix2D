cmake_minimum_required(VERSION 3.16)

find_package(SDL2 REQUIRED)

add_library(glad STATIC ${CMAKE_SOURCE_DIR}/external/glad/src/glad.cpp)
target_include_directories(glad PUBLIC ${CMAKE_SOURCE_DIR}/external/glad/include)

add_library(k2d STATIC
    src/Input.cpp
    src/FileBuffer.cpp
    src/Device.cpp
    src/Shader.cpp
    src/Texture.cpp
    src/Assets.cpp
    src/Batch.cpp
    src/Component.cpp
    src/GameObject.cpp
    src/Scene.cpp
    src/SpriteComponent.cpp
)

target_include_directories(k2d PUBLIC
    include
    ${CMAKE_SOURCE_DIR}/external/glad/include
    ${CMAKE_SOURCE_DIR}/external/stb
    /media/projectos/projects/cpp/containers/include
)

target_link_libraries(k2d PUBLIC glad glm::glm)

if(TARGET SDL2::SDL2)
    target_link_libraries(k2d PUBLIC SDL2::SDL2)
else()
    target_include_directories(k2d PUBLIC ${SDL2_INCLUDE_DIRS})
    target_link_libraries(k2d PUBLIC ${SDL2_LIBRARIES})
endif()

target_precompile_headers(k2d PRIVATE src/pch.h)
