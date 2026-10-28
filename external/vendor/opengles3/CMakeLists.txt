# The OpenGL ES 3 loader from ZenHuman (src/render/OpenGLES3.*), for desktop GL
# and GLES contexts; the functions come from the platform's loader only.
add_library(opengles3 STATIC OpenGLES3.cpp)
target_include_directories(opengles3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(opengles3 PRIVATE GLES3_LOADER_ONLY)
target_compile_features(opengles3 PRIVATE cxx_std_14)
