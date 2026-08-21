cmake_minimum_required(VERSION 3.16)

add_library(kx STATIC
    src/shapes.cpp
    src/manifold.cpp
    src/collide.cpp
    src/body.cpp
    src/joint.cpp
    src/dynamictree.cpp
    src/world.cpp
    src/mousejoint.cpp
    src/debugdraw.cpp
)

target_include_directories(kx PUBLIC
    include
    ${CMAKE_SOURCE_DIR}/engine/include
    /media/projectos/projects/cpp/containers/include
)

target_link_libraries(kx PUBLIC glm::glm)
