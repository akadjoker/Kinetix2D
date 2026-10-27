# The engine, scripting, runner and tools use the ct containers, never the STL
# ones. This is not a build target: it reads the sources and stops the build on
# the first STL container, smart pointer, stream, thread or filesystem use.
file(GLOB_RECURSE K2D_SOURCES
     "${K2D_SOURCE_DIR}/engine/src/*.cpp"
     "${K2D_SOURCE_DIR}/engine/include/*.h"
     "${K2D_SOURCE_DIR}/physics2d/*.cpp"
     "${K2D_SOURCE_DIR}/physics2d/*.h"
     "${K2D_SOURCE_DIR}/scripting/*.cpp"
     "${K2D_SOURCE_DIR}/scripting/*.h"
     "${K2D_SOURCE_DIR}/runner/*.cpp"
     "${K2D_SOURCE_DIR}/tools/*.cpp")
list(FILTER K2D_SOURCES EXCLUDE REGEX "/engine/src/audio/")

set(K2D_STL_TYPES "string|wstring|string_view|vector|map|multimap|unordered_map|unordered_set|set|multiset|list|forward_list|deque|queue|stack|array|function|unique_ptr|shared_ptr|weak_ptr|optional|variant|any|thread|mutex|filesystem|stringstream|istringstream|ostringstream|ifstream|ofstream|fstream")
set(K2D_STL_HEADERS "vector|string|string_view|map|unordered_map|set|unordered_set|list|forward_list|deque|queue|stack|array|functional|memory|optional|variant|any|thread|mutex|filesystem|sstream|fstream")

foreach(K2D_FILE IN LISTS K2D_SOURCES)
    file(READ "${K2D_FILE}" K2D_CONTENT)
    string(REGEX MATCH "std[ \t\r\n]*::[ \t\r\n]*(${K2D_STL_TYPES})[^A-Za-z0-9_]" K2D_MATCH "${K2D_CONTENT}")
    if(K2D_MATCH)
        message(FATAL_ERROR "STL container use in ${K2D_FILE}: ${K2D_MATCH}")
    endif()
    string(REGEX MATCH "#[ \t]*include[ \t]*[<\"](${K2D_STL_HEADERS})[>\"]" K2D_MATCH "${K2D_CONTENT}")
    if(K2D_MATCH)
        message(FATAL_ERROR "STL header in ${K2D_FILE}: ${K2D_MATCH}")
    endif()
endforeach()

message(STATUS "k2d_check_no_stl: no STL containers in engine, scripting, runner and tools")
