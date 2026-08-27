cmake_minimum_required(VERSION 3.16)

set(PHYS_REF_DIR /media/projectos/projects/cpp/vibecoding/fisica/phys)

if(EXISTS "${PHYS_REF_DIR}/src/collide.cpp")
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
else()
    message(STATUS "Skipping external fidelity tests: PHYS_REF_DIR is not available")
endif()

add_executable(kx_solver_tests kx_solver_tests.cpp)
target_link_libraries(kx_solver_tests PRIVATE kx)

add_executable(kx_broadphase_tests kx_broadphase_tests.cpp)
target_link_libraries(kx_broadphase_tests PRIVATE kx)

add_executable(kx_imageshape_tests kx_imageshape_tests.cpp)
target_link_libraries(kx_imageshape_tests PRIVATE kx)

add_executable(kx_queries_tests kx_queries_tests.cpp)
target_link_libraries(kx_queries_tests PRIVATE kx)

add_executable(kx_shapecast_tests kx_shapecast_tests.cpp)
target_link_libraries(kx_shapecast_tests PRIVATE kx)

add_executable(kx_sleep_tests kx_sleep_tests.cpp)
target_link_libraries(kx_sleep_tests PRIVATE kx)

add_executable(kx_events_tests kx_events_tests.cpp)
target_link_libraries(kx_events_tests PRIVATE kx)

add_executable(kx_motorjoint_tests kx_motorjoint_tests.cpp)
target_link_libraries(kx_motorjoint_tests PRIVATE kx)

add_executable(kx_fixedrotation_tests kx_fixedrotation_tests.cpp)
target_link_libraries(kx_fixedrotation_tests PRIVATE kx)

add_executable(kx_platformer_tests kx_platformer_tests.cpp)
target_link_libraries(kx_platformer_tests PRIVATE kx)

add_executable(k2d_particle_tests k2d_particle_tests.cpp)
target_link_libraries(k2d_particle_tests PRIVATE k2d)

add_executable(k2d_spritebatch_tests k2d_spritebatch_tests.cpp)
target_link_libraries(k2d_spritebatch_tests PRIVATE k2d)

add_executable(k2d_polygon2d_tests k2d_polygon2d_tests.cpp)
target_link_libraries(k2d_polygon2d_tests PRIVATE k2d)

add_executable(k2d_scene_tests k2d_scene_tests.cpp)
target_link_libraries(k2d_scene_tests PRIVATE k2d)

add_executable(k2d_camera_tests k2d_camera_tests.cpp)
target_link_libraries(k2d_camera_tests PRIVATE k2d)

add_executable(k2d_kpak_tests k2d_kpak_tests.cpp)
target_link_libraries(k2d_kpak_tests PRIVATE k2d)

add_executable(k2d_material_tests k2d_material_tests.cpp)
target_link_libraries(k2d_material_tests PRIVATE k2d)

add_executable(kx_tilemap_tests kx_tilemap_tests.cpp)
target_link_libraries(kx_tilemap_tests PRIVATE kx)

add_executable(kx_review_fixes_tests kx_review_fixes_tests.cpp)
target_link_libraries(kx_review_fixes_tests PRIVATE kx)

add_executable(k2d_animation_tests k2d_animation_tests.cpp)
target_link_libraries(k2d_animation_tests PRIVATE k2d)

add_executable(k2d_skeleton_tests k2d_skeleton_tests.cpp)
target_link_libraries(k2d_skeleton_tests PRIVATE k2d)

add_executable(k2d_zen_tests k2d_zen_tests.cpp)
target_link_libraries(k2d_zen_tests PRIVATE k2d_zen)
target_compile_features(k2d_zen_tests PRIVATE cxx_std_17)

add_executable(k2d_zen_class_tests k2d_zen_class_tests.cpp)
target_link_libraries(k2d_zen_class_tests PRIVATE k2d_zen)
target_compile_features(k2d_zen_class_tests PRIVATE cxx_std_17)

add_executable(k2d_zen_bytecode_tests k2d_zen_bytecode_tests.cpp)
target_link_libraries(k2d_zen_bytecode_tests PRIVATE k2d_zen)
target_compile_features(k2d_zen_bytecode_tests PRIVATE cxx_std_17)

add_executable(k2d_zen_props_tests k2d_zen_props_tests.cpp)
target_link_libraries(k2d_zen_props_tests PRIVATE k2d_zen)
target_compile_features(k2d_zen_props_tests PRIVATE cxx_std_17)

add_executable(k2d_zen_prefab_tests k2d_zen_prefab_tests.cpp)
target_link_libraries(k2d_zen_prefab_tests PRIVATE k2d_zen)
target_compile_features(k2d_zen_prefab_tests PRIVATE cxx_std_17)

add_executable(k2d_pixmap_tests k2d_pixmap_tests.cpp)
target_link_libraries(k2d_pixmap_tests PRIVATE k2d)

add_executable(k2d_astar_tests k2d_astar_tests.cpp)
target_link_libraries(k2d_astar_tests PRIVATE k2d)

add_executable(k2d_navigation_tests k2d_navigation_tests.cpp)
target_link_libraries(k2d_navigation_tests PRIVATE k2d)

add_executable(k2d_serializer_tests k2d_serializer_tests.cpp)
target_link_libraries(k2d_serializer_tests PRIVATE k2d)

add_executable(physics_reference_test physics_reference_test.cpp)
target_link_libraries(physics_reference_test PRIVATE kx)

add_executable(k2d_multicomponent_tests k2d_multicomponent_tests.cpp)
target_link_libraries(k2d_multicomponent_tests PRIVATE k2d)

add_executable(k2d_prefab_tests k2d_prefab_tests.cpp)
target_link_libraries(k2d_prefab_tests PRIVATE k2d)

add_executable(k2d_physics2d_tests k2d_physics2d_tests.cpp)
target_link_libraries(k2d_physics2d_tests PRIVATE k2d_physics2d)
target_compile_features(k2d_physics2d_tests PRIVATE cxx_std_17)

add_executable(k2d_performance_benchmark k2d_performance_benchmark.cpp)
target_link_libraries(k2d_performance_benchmark PRIVATE k2d k2d_physics2d kx)
target_compile_features(k2d_performance_benchmark PRIVATE cxx_std_17)

add_executable(k2d_zen_physics_tests k2d_zen_physics_tests.cpp)
target_link_libraries(k2d_zen_physics_tests PRIVATE k2d_zen k2d_physics2d)
target_compile_features(k2d_zen_physics_tests PRIVATE cxx_std_17)

set(K2D_ZEN_TEST_TARGETS
    k2d_zen_tests
    k2d_zen_class_tests
    k2d_zen_bytecode_tests
    k2d_zen_props_tests
    k2d_zen_prefab_tests
    k2d_zen_physics_tests
)

set(K2D_TEST_TARGETS
    k2d_particle_tests
    k2d_spritebatch_tests
    k2d_polygon2d_tests
    k2d_scene_tests
    k2d_camera_tests
    k2d_kpak_tests
    k2d_material_tests
    k2d_animation_tests
    k2d_skeleton_tests
    k2d_pixmap_tests
    k2d_astar_tests
    k2d_navigation_tests
    k2d_serializer_tests
    k2d_multicomponent_tests
    k2d_prefab_tests
    k2d_physics2d_tests
)

set(KX_TEST_TARGETS
    kx_solver_tests
    kx_broadphase_tests
    kx_imageshape_tests
    kx_queries_tests
    kx_shapecast_tests
    kx_sleep_tests
    kx_events_tests
    kx_motorjoint_tests
    kx_fixedrotation_tests
    kx_platformer_tests
    kx_tilemap_tests
    kx_review_fixes_tests
    physics_reference_test
)

if(TARGET kx_collision_tests)
    list(APPEND KX_TEST_TARGETS kx_collision_tests)
endif()

set(K2D_ALL_TEST_TARGETS ${K2D_ZEN_TEST_TARGETS} ${K2D_TEST_TARGETS} ${KX_TEST_TARGETS})
foreach(test_target IN LISTS K2D_ALL_TEST_TARGETS)
    if(TARGET ${test_target})
        add_test(NAME ${test_target} COMMAND ${test_target})
        set_tests_properties(${test_target} PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    endif()
endforeach()
