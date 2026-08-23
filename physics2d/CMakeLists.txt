add_library(k2d_physics2d STATIC
    src/Collider2D.cpp
    src/PhysicsWorld2D.cpp
    src/RigidBody2D.cpp
)

target_include_directories(k2d_physics2d PUBLIC include)
target_link_libraries(k2d_physics2d PUBLIC k2d kx)
