if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/upstream/libzen/CMakeLists.txt)
    message(FATAL_ERROR
        "external/zen/upstream is empty — run: git submodule update --init --recursive")
endif()

# The editor routes print() and runtime errors into its Console; without this
# zenconf.h writes them to stdout and ZenScriptComponent fails to link.
set(ZEN_HOST_OUTPUT ON CACHE BOOL "" FORCE)

# zen_debug, the VM with the debug hook, only for k2d_runner_debug: the game,
# the editor and every export link zen_static, which has none.
if(NOT EMSCRIPTEN AND NOT ANDROID)
    set(ZEN_BUILD_DEBUG_LIBRARY ON CACHE BOOL "" FORCE)
endif()

add_subdirectory(upstream/libzen)
