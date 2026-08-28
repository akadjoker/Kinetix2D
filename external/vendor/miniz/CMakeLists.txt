# Vendored miniz 3.1.2 (77d0dce).  Keep this target intentionally small:
# Kinetix needs only a static deflate/ZIP implementation for KPAK files.
add_library(miniz STATIC
    miniz.c
    miniz_tdef.c
    miniz_tinfl.c
    miniz_zip.c
)
target_include_directories(miniz PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(miniz PROPERTIES
    C_STANDARD 90
    C_STANDARD_REQUIRED ON
)
