cmake_minimum_required(VERSION 3.16)

add_executable(k2d_particle_tests k2d_particle_tests.cpp)
target_link_libraries(k2d_particle_tests PRIVATE k2d)

add_executable(k2d_particle_physics_tests k2d_particle_physics_tests.cpp)
target_link_libraries(k2d_particle_physics_tests PRIVATE k2d_physics2d)
target_compile_features(k2d_particle_physics_tests PRIVATE cxx_std_17)

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

add_executable(k2d_multicomponent_tests k2d_multicomponent_tests.cpp)
target_link_libraries(k2d_multicomponent_tests PRIVATE k2d)

add_executable(k2d_prefab_tests k2d_prefab_tests.cpp)
target_link_libraries(k2d_prefab_tests PRIVATE k2d)

add_executable(k2d_physics2d_tests k2d_physics2d_tests.cpp)
target_link_libraries(k2d_physics2d_tests PRIVATE k2d_physics2d)
target_compile_features(k2d_physics2d_tests PRIVATE cxx_std_17)

add_executable(k2d_collider_points_tests k2d_collider_points_tests.cpp)
target_link_libraries(k2d_collider_points_tests PRIVATE k2d_physics2d)
target_compile_features(k2d_collider_points_tests PRIVATE cxx_std_17)

add_executable(k2d_performance_benchmark k2d_performance_benchmark.cpp)
target_link_libraries(k2d_performance_benchmark PRIVATE k2d k2d_physics2d)
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
    k2d_particle_physics_tests
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
    k2d_collider_points_tests
)

set(K2D_ALL_TEST_TARGETS ${K2D_ZEN_TEST_TARGETS} ${K2D_TEST_TARGETS})
foreach(test_target IN LISTS K2D_ALL_TEST_TARGETS)
    if(TARGET ${test_target})
        add_test(NAME ${test_target} COMMAND ${test_target})
        set_tests_properties(${test_target} PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    endif()
endforeach()

add_executable(k2d_character_tests k2d_character_tests.cpp)
target_link_libraries(k2d_character_tests PRIVATE k2d_physics2d)
