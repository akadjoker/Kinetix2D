cmake_minimum_required(VERSION 3.10)
project(libzen LANGUAGES CXX)
 

# --- Vendor libraries (conditional) ---
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(ZEN_VENDOR_LIBS "")
set(ZEN_VENDOR_INCLUDES "")
set(ZEN_DEFINITIONS "")
 
 
# Core library sources
file(GLOB CORE_SRCS CONFIGURE_DEPENDS src/*.cpp)

# --- libzen (static library) ---
add_library(zen_static STATIC ${CORE_SRCS})
target_include_directories(zen_static PUBLIC include include/zen PRIVATE src ${ZEN_VENDOR_INCLUDES})
target_compile_options(zen_static PRIVATE -include zen/zen_host_output.h)
target_link_libraries(zen_static PUBLIC ${ZEN_VENDOR_LIBS} ${CMAKE_DL_LIBS})

if(WIN32)
    target_link_libraries(zen_static PUBLIC ws2_32)
endif()
target_compile_definitions(zen_static PUBLIC ${ZEN_DEFINITIONS})
target_compile_options(zen_static PRIVATE $<$<CONFIG:Release>:-O3>)
target_compile_definitions(zen_static PRIVATE $<$<CONFIG:Release>:NDEBUG>)
set_target_properties(zen_static PROPERTIES OUTPUT_NAME zen POSITION_INDEPENDENT_CODE ON)

# --- libzen (shared library) ---
# add_library(zen_shared SHARED ${CORE_SRCS})
# target_include_directories(zen_shared PUBLIC include include/zen PRIVATE src ${ZEN_VENDOR_INCLUDES})
# target_link_libraries(zen_shared PUBLIC ${ZEN_VENDOR_LIBS} ${CMAKE_DL_LIBS})
# target_compile_definitions(zen_shared PUBLIC ${ZEN_DEFINITIONS})
# target_compile_options(zen_shared PRIVATE $<$<CONFIG:Release>:-O3>)
# target_compile_definitions(zen_shared PRIVATE $<$<CONFIG:Release>:NDEBUG>)
# set_target_properties(zen_shared PROPERTIES OUTPUT_NAME zen)
