#include "k2d/OpenGL.h"

#include <platform.h>

namespace k2d
{

bool LoadOpenGL()
{
#if K2D_OPENGL_NATIVE_GLES3
    return true;
#else
    glesSetProcAddressLoader(gl_proc_address);
    if (!initOpenGLExtensions())
    {
        log_error("The GL context lacks OpenGL ES 3 functions");
        return false;
    }
    if (glESExt::majorVersion < 3)
    {
        log_error("OpenGL %d.%d is older than 3.0", glESExt::majorVersion, glESExt::minorVersion);
        return false;
    }
    return true;
#endif
}

} // namespace k2d
