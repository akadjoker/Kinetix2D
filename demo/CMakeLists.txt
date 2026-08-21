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

add_executable(particle_fx_demo particle_fx_demo.cpp)
target_link_libraries(particle_fx_demo PRIVATE k2d imgui)

add_executable(parallax_demo parallax_demo.cpp)
target_link_libraries(parallax_demo PRIVATE k2d imgui)

add_executable(canvas_bunnymark canvas_bunnymark.cpp)
target_link_libraries(canvas_bunnymark PRIVATE k2d imgui)

add_executable(assets_demo assets_demo.cpp)
target_link_libraries(assets_demo PRIVATE k2d imgui)

add_executable(testbed
    testbed/testbed.cpp
    testbed/demos_basic.cpp
    testbed/demos_joints.cpp
    testbed/demos_misc.cpp
    testbed/demos_more.cpp)
target_include_directories(testbed PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/testbed)
target_link_libraries(testbed PRIVATE k2d kx imgui)
