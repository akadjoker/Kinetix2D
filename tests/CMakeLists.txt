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

add_executable(kx_broadphase_tests kx_broadphase_tests.cpp)
target_link_libraries(kx_broadphase_tests PRIVATE kx)

set(POLY2TRI_DIR ${CMAKE_SOURCE_DIR}/external/poly2tri)

add_executable(kx_triangulate_tests
    kx_triangulate_tests.cpp
    ${POLY2TRI_DIR}/src/common/shapes.cc
    ${POLY2TRI_DIR}/src/sweep/advancing_front.cc
    ${POLY2TRI_DIR}/src/sweep/cdt.cc
    ${POLY2TRI_DIR}/src/sweep/sweep.cc
    ${POLY2TRI_DIR}/src/sweep/sweep_context.cc
)
target_include_directories(kx_triangulate_tests PRIVATE ${POLY2TRI_DIR}/include)
target_link_libraries(kx_triangulate_tests PRIVATE kx)
set_source_files_properties(
    ${POLY2TRI_DIR}/src/sweep/sweep.cc
    ${POLY2TRI_DIR}/src/sweep/sweep_context.cc
    PROPERTIES COMPILE_OPTIONS "-Wno-address;-Wno-nonnull-compare"
)

add_executable(kx_imageshape_tests kx_imageshape_tests.cpp)
target_link_libraries(kx_imageshape_tests PRIVATE kx)

add_executable(kx_queries_tests kx_queries_tests.cpp)
target_link_libraries(kx_queries_tests PRIVATE kx)

add_executable(kx_sleep_tests kx_sleep_tests.cpp)
target_link_libraries(kx_sleep_tests PRIVATE kx)

add_executable(k2d_particle_tests k2d_particle_tests.cpp)
target_link_libraries(k2d_particle_tests PRIVATE k2d)

add_executable(k2d_spritebatch_tests k2d_spritebatch_tests.cpp)
target_link_libraries(k2d_spritebatch_tests PRIVATE k2d)

add_executable(k2d_polygon2d_tests k2d_polygon2d_tests.cpp)
target_link_libraries(k2d_polygon2d_tests PRIVATE k2d)
