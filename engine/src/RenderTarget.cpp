#include "k2d/RenderTarget.h"

#include "k2d/OpenGL.h"

#include <platform.h>

namespace k2d
{

RenderTarget::~RenderTarget()
{
    destroy();
}

bool RenderTarget::resize(int width, int height)
{
    if (width <= 0 || height <= 0)
        return false;
    if (mFramebuffer && width == mWidth && height == mHeight)
        return true;
    destroy();

    glGenTextures(1, &mTexture);
    glBindTexture(GL_TEXTURE_2D, mTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenRenderbuffers(1, &mDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, mDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glGenFramebuffers(1, &mFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTexture, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, mDepth);
    const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!complete)
    {
        log_error("render target %dx%d is incomplete", width, height);
        destroy();
        return false;
    }
    mWidth = width;
    mHeight = height;
    return true;
}

void RenderTarget::destroy()
{
    if (mFramebuffer)
        glDeleteFramebuffers(1, &mFramebuffer);
    if (mDepth)
        glDeleteRenderbuffers(1, &mDepth);
    if (mTexture)
        glDeleteTextures(1, &mTexture);
    mFramebuffer = mTexture = mDepth = 0;
    mWidth = mHeight = 0;
}

void RenderTarget::bind() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    glViewport(0, 0, mWidth, mHeight);
}

} // namespace k2d
