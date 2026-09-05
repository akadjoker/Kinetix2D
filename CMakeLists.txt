cmake_minimum_required(VERSION 3.16)
project(Kinetix2D C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING
        "Use the static MSVC runtime so release binaries do not require the VC++ Redistributable")
endif()

set(K2D_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin" CACHE PATH
    "Directory where Kinetix2D executables are written")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${K2D_RUNTIME_OUTPUT_DIRECTORY}")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(K2D_SANITIZERS_DEFAULT ON)
else()
    set(K2D_SANITIZERS_DEFAULT OFF)
endif()

option(K2D_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer"
       ${K2D_SANITIZERS_DEFAULT})
option(K2D_STATIC_MINGW_RUNTIME "Link MinGW GCC runtimes into Kinetix executables" ON)

# option() only honours its default the first time a build directory is
# configured, so a cache that once had sanitizers on keeps them through a
# later switch to Release -- a 10x slowdown with nothing in the output to say
# why. A release build never wants them, so the build type overrules the cache.
if(K2D_ENABLE_SANITIZERS AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Sanitizers disabled: only a Debug build runs them (CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})")
    set(K2D_ENABLE_SANITIZERS OFF CACHE BOOL "Enable AddressSanitizer and UndefinedBehaviorSanitizer" FORCE)
endif()

if(K2D_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(STATUS "Sanitizers ON (AddressSanitizer + UndefinedBehaviorSanitizer) -- expect a large slowdown")
        # -g keeps the ASan report's frames symbolised even if the build type
        # never added it; the trap on UB error turns a silent miscompile into
        # a stack the debugger can stop on.
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
        add_link_options(-fsanitize=address,undefined)
    else()
        message(WARNING "K2D_ENABLE_SANITIZERS requires a Clang or GNU compiler")
    endif()
endif()

# MinGW's GCC runtime is otherwise a DLL dependency (libgcc_s_seh-1.dll,
# libstdc++-6.dll, and libwinpthread-1.dll on posix-threaded toolchains),
# which makes a copied runner fail on a clean Windows machine. -static pulls
# all three into the binary; SDL and other third-party DLLs, if any remain
# for a given toolchain, stay app-local and are copied by the release workflow.
if(MINGW AND K2D_STATIC_MINGW_RUNTIME)
    add_link_options(-static-libgcc -static-libstdc++ -static)
endif()


find_package(SDL2 CONFIG REQUIRED)
set(MATHC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MATHC_BUILD_BENCH OFF CACHE BOOL "" FORCE)
set(MATHC_NATIVE_ARCH OFF CACHE BOOL "" FORCE)
add_subdirectory(external/math)
add_subdirectory(external/vendor/miniz)

add_subdirectory(external/zen)

add_subdirectory(engine)
add_subdirectory(tools)
add_subdirectory(scripting)
add_subdirectory(physics2d)
add_subdirectory(runner)
#add_subdirectory(demo)
add_subdirectory(editor)

include(GNUInstallDirs)
install(TARGETS k2d_editor k2d_runner k2d_pack k2d_scriptc k2d_webserver RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS tools/export_web.sh tools/export_web.py DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES tools/export_web.bat DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY assets DESTINATION .)
install(FILES README.md DESTINATION .)

enable_testing()
add_subdirectory(tests)
