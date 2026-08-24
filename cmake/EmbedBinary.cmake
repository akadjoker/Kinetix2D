# Converts one small engine-owned binary asset into a C++ header. This is
# deliberately a build step rather than a checked-in generated file: changing
# the source PNG cannot leave stale bytes in the executable.
if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "EmbedBinary.cmake requires INPUT and OUTPUT")
endif()

file(READ "${INPUT}" K2D_EMBED_HEX HEX)
string(LENGTH "${K2D_EMBED_HEX}" K2D_EMBED_HEX_LENGTH)
math(EXPR K2D_EMBED_BYTE_COUNT "${K2D_EMBED_HEX_LENGTH} / 2")
get_filename_component(K2D_EMBED_DIRECTORY "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${K2D_EMBED_DIRECTORY}")

set(K2D_EMBED_DATA "")
if(K2D_EMBED_BYTE_COUNT GREATER 0)
    math(EXPR K2D_EMBED_LAST_BYTE "${K2D_EMBED_BYTE_COUNT} - 1")
    foreach(K2D_EMBED_INDEX RANGE 0 ${K2D_EMBED_LAST_BYTE})
        math(EXPR K2D_EMBED_OFFSET "${K2D_EMBED_INDEX} * 2")
        string(SUBSTRING "${K2D_EMBED_HEX}" ${K2D_EMBED_OFFSET} 2 K2D_EMBED_BYTE)
        string(APPEND K2D_EMBED_DATA "0x${K2D_EMBED_BYTE},")
        math(EXPR K2D_EMBED_LINE_BREAK "${K2D_EMBED_INDEX} % 16")
        if(K2D_EMBED_LINE_BREAK EQUAL 15)
            string(APPEND K2D_EMBED_DATA "\n")
        endif()
    endforeach()
endif()

file(WRITE "${OUTPUT}" "#pragma once\n\n#include <cstddef>\n\nnamespace k2d\n{\nstatic const unsigned char kDefaultUiThemePng[] = {\n${K2D_EMBED_DATA}\n};\nstatic const std::size_t kDefaultUiThemePngSize = sizeof(kDefaultUiThemePng);\n}\n")
