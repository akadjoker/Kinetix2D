add_library(k2d_physics2d STATIC
    src/BoxCollider2D.cpp
    src/ChainCollider2D.cpp
    src/CircleCollider2D.cpp
    src/Collider2D.cpp
    src/EdgeCollider2D.cpp
    src/Physics2DSerializer.cpp
    src/PhysicsWorld2D.cpp
    src/PolygonCollider2D.cpp
    src/RigidBody2D.cpp
)

target_include_directories(k2d_physics2d PUBLIC include)
target_link_libraries(k2d_physics2d PUBLIC k2d kx)
