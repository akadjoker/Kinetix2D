# Cross-compilation toolchain for a portable Linux-hosted MinGW-w64 SDK.
#
# The SDK is deliberately external to the repository. Set K2D_MINGW_ROOT to
# its root directory; it must contain bin/x86_64-w64-mingw32-g++.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# The compiler-ABI try_compile re-runs this file in a scratch project that
# only inherits variables listed here, so without this the guard below fails
# on a fresh build directory even though the outer configure passed it.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES K2D_MINGW_ROOT)

if(NOT K2D_MINGW_ROOT)
    set(K2D_MINGW_ROOT "$ENV{K2D_MINGW_ROOT}")
endif()

if(NOT K2D_MINGW_ROOT)
    message(FATAL_ERROR "Set K2D_MINGW_ROOT to the portable MinGW-w64 SDK root")
endif()

set(_k2d_mingw_bin "${K2D_MINGW_ROOT}/bin")
set(CMAKE_C_COMPILER "${_k2d_mingw_bin}/x86_64-w64-mingw32-gcc" CACHE FILEPATH "")
set(CMAKE_CXX_COMPILER "${_k2d_mingw_bin}/x86_64-w64-mingw32-g++" CACHE FILEPATH "")
set(CMAKE_RC_COMPILER "${_k2d_mingw_bin}/x86_64-w64-mingw32-windres" CACHE FILEPATH "")

foreach(_k2d_compiler IN ITEMS CMAKE_C_COMPILER CMAKE_CXX_COMPILER CMAKE_RC_COMPILER)
    if(NOT EXISTS "${${_k2d_compiler}}")
        message(FATAL_ERROR "Portable MinGW-w64 SDK is incomplete: ${${_k2d_compiler}} is missing")
    endif()
endforeach()

list(FIND CMAKE_FIND_ROOT_PATH "${K2D_MINGW_ROOT}" _k2d_mingw_root_index)
if(_k2d_mingw_root_index EQUAL -1)
    list(APPEND CMAKE_FIND_ROOT_PATH "${K2D_MINGW_ROOT}")
endif()
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
