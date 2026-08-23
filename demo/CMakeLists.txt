cmake_minimum_required(VERSION 3.16)

add_executable(demo main.cpp)
target_link_libraries(demo PRIVATE k2d)

add_executable(bunnymark bunnymark.cpp)
target_link_libraries(bunnymark PRIVATE k2d)

add_executable(bunnymark_raw bunnymark_raw.cpp)
target_link_libraries(bunnymark_raw PRIVATE k2d)

add_executable(physics_demo physics_demo.cpp)
target_link_libraries(physics_demo PRIVATE k2d kx imgui)

add_executable(stress_demo stress_demo.cpp)
target_link_libraries(stress_demo PRIVATE k2d kx imgui)

add_executable(render_demo render_demo.cpp)
target_link_libraries(render_demo PRIVATE k2d)

add_executable(render_pass_demo render_pass_demo.cpp)
target_link_libraries(render_pass_demo PRIVATE k2d)

add_executable(lighting_nodes_demo lighting_nodes_demo.cpp)
target_link_libraries(lighting_nodes_demo PRIVATE k2d)

add_executable(light_falloff_diag light_falloff_diag.cpp)
target_link_libraries(light_falloff_diag PRIVATE k2d)

add_executable(tiling_diag tiling_diag.cpp)
target_link_libraries(tiling_diag PRIVATE k2d)

add_executable(line_ninepatch_diag line_ninepatch_diag.cpp)
target_link_libraries(line_ninepatch_diag PRIVATE k2d)

add_executable(screenfade_diag screenfade_diag.cpp)
target_link_libraries(screenfade_diag PRIVATE k2d)

add_executable(scenedebug_diag scenedebug_diag.cpp)
target_link_libraries(scenedebug_diag PRIVATE k2d)

add_executable(cullmask_diag cullmask_diag.cpp)
target_link_libraries(cullmask_diag PRIVATE k2d)

add_executable(normalmap_diag normalmap_diag.cpp)
target_link_libraries(normalmap_diag PRIVATE k2d)

add_executable(canvaslayer_diag canvaslayer_diag.cpp)
target_link_libraries(canvaslayer_diag PRIVATE k2d)

add_executable(serializer_diag serializer_diag.cpp)
target_link_libraries(serializer_diag PRIVATE k2d)

add_executable(polygon_uv_diag polygon_uv_diag.cpp)
target_link_libraries(polygon_uv_diag PRIVATE k2d)

add_executable(customshader_diag customshader_diag.cpp)
target_link_libraries(customshader_diag PRIVATE k2d)

add_executable(particle_fx_demo particle_fx_demo.cpp)
target_link_libraries(particle_fx_demo PRIVATE k2d imgui)

add_executable(prefab_bullet_demo prefab_bullet_demo.cpp)
target_link_libraries(prefab_bullet_demo PRIVATE k2d imgui)

add_executable(parallax_demo parallax_demo.cpp)
target_link_libraries(parallax_demo PRIVATE k2d imgui)

add_executable(canvas_bunnymark canvas_bunnymark.cpp)
target_link_libraries(canvas_bunnymark PRIVATE k2d imgui)

add_executable(assets_demo assets_demo.cpp)
target_link_libraries(assets_demo PRIVATE k2d imgui)

add_executable(testbed
    testbed/testbed.cpp
    testbed/demos_basic.cpp
    testbed/demos_stacking.cpp
    testbed/demos_box2d_joints.cpp
    testbed/demos_events.cpp
    testbed/demos_joints.cpp
    testbed/demos_misc.cpp
    testbed/demos_more.cpp
    testbed/demos_platformer.cpp)
target_include_directories(testbed PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/testbed)
target_link_libraries(testbed PRIVATE k2d kx imgui)
