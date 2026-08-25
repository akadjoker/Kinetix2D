# Converts one small engine-owned binary asset into a C++ header. This is
# deliberately a build step rather than a checked-in generated file: changing
# the source PNG cannot leave stale bytes in the executable.
if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "EmbedBinary.cmake requires INPUT and OUTPUT")
endif()

if(NOT DEFINED VARIABLE)
    set(VARIABLE "kDefaultUiThemePng")
endif()

file(READ "${INPUT}" K2D_EMBED_HEX HEX)
string(LENGTH "${K2D_EMBED_HEX}" K2D_EMBED_HEX_LENGTH)
math(EXPR K2D_EMBED_BYTE_COUNT "${K2D_EMBED_HEX_LENGTH} / 2")
get_filename_component(K2D_EMBED_DIRECTORY "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${K2D_EMBED_DIRECTORY}")

set(K2D_EMBED_DATA "")
if(K2D_EMBED_BYTE_COUNT GREATER 0)
    # One regular-expression pass is dramatically faster for larger embedded
    # assets (such as the mobile virtual pad) than appending every byte in a
    # CMake loop.
    string(REGEX REPLACE "([0-9A-Fa-f][0-9A-Fa-f])" "0x\\1," K2D_EMBED_DATA "${K2D_EMBED_HEX}")
endif()

file(WRITE "${OUTPUT}" "#pragma once\n\n#include <cstddef>\n\nnamespace k2d\n{\nstatic const unsigned char ${VARIABLE}[] = {\n${K2D_EMBED_DATA}\n};\nstatic const std::size_t ${VARIABLE}Size = sizeof(${VARIABLE});\n}\n")
