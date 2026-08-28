if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/upstream/libzen/CMakeLists.txt)
    message(FATAL_ERROR
        "external/zen/upstream is empty — run: git submodule update --init --recursive")
endif()

# The editor routes print() and runtime errors into its Console; without this
# zenconf.h writes them to stdout and ZenScriptComponent fails to link.
set(ZEN_HOST_OUTPUT ON CACHE BOOL "" FORCE)

add_subdirectory(upstream/libzen)
