cmake_minimum_required(VERSION 3.16)

add_executable(demo main.cpp)
target_link_libraries(demo PRIVATE k2d)

add_executable(bunnymark bunnymark.cpp)
target_link_libraries(bunnymark PRIVATE k2d)

add_executable(bunnymark_raw bunnymark_raw.cpp)
target_link_libraries(bunnymark_raw PRIVATE k2d)

add_executable(physics_demo physics_demo.cpp)
target_link_libraries(physics_demo PRIVATE k2d kx)
