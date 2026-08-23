cmake_minimum_required(VERSION 3.16)
project(Kinetix2D CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(MATHC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MATHC_BUILD_BENCH OFF CACHE BOOL "" FORCE)
set(MATHC_NATIVE_ARCH OFF CACHE BOOL "" FORCE)
add_subdirectory(external/math)

add_subdirectory(external/zen)

add_subdirectory(engine)
add_subdirectory(scripting)
add_subdirectory(physics)
add_subdirectory(physics2d)
add_subdirectory(demo)
add_subdirectory(editor)

enable_testing()
add_subdirectory(tests)
