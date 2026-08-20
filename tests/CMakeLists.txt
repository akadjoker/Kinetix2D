cmake_minimum_required(VERSION 3.16)

set(PHYS_REF_DIR /media/projectos/projects/cpp/vibecoding/fisica/phys)

add_executable(kx_fidelity
    kx_fidelity.cpp
    ${PHYS_REF_DIR}/src/collide.cpp
    ${PHYS_REF_DIR}/src/shapes.cpp
    ${PHYS_REF_DIR}/src/manifold.cpp
    ${PHYS_REF_DIR}/src/math.cpp
)
target_include_directories(kx_fidelity PRIVATE ${PHYS_REF_DIR}/include)
target_link_libraries(kx_fidelity PRIVATE kx)

add_executable(kx_collision_tests
    kx_collision_tests.cpp
    ${PHYS_REF_DIR}/src/collide.cpp
    ${PHYS_REF_DIR}/src/shapes.cpp
    ${PHYS_REF_DIR}/src/manifold.cpp
    ${PHYS_REF_DIR}/src/math.cpp
)
target_include_directories(kx_collision_tests PRIVATE ${PHYS_REF_DIR}/include)
target_link_libraries(kx_collision_tests PRIVATE kx)

add_executable(kx_solver_tests kx_solver_tests.cpp)
target_link_libraries(kx_solver_tests PRIVATE kx)
