#pragma once

// One OpenGL ES 3 API for every Kinetix2D renderer. WebGL 2 and Android expose
// GLES directly; desktop loads the same entry points from the platform's GL
// context into the gles namespace.
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
#define K2D_OPENGL_NATIVE_GLES3 1
#include <GLES3/gl3.h>
#else
#define K2D_OPENGL_NATIVE_GLES3 0
#include <OpenGLES3.h>
using namespace gles;
#endif

namespace k2d
{
// Loads the GL functions of the current context; whoever makes the context
// calls it once before anything draws.
bool LoadOpenGL();
} // namespace k2d
