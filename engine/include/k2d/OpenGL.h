#pragma once

// One OpenGL ES 3 API for every Kinetix2D renderer. WebGL 2 and Android expose
// GLES directly; desktop resolves the same entry points through the platform's
// GL loader.
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
#define K2D_OPENGL_NATIVE_GLES3 1
#include <GLES3/gl3.h>
#else
#define K2D_OPENGL_NATIVE_GLES3 0
#include <glad/gl.h>
#endif
