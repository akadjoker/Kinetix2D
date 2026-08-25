#include "k2d/RenderState.h"

#include <cstring>

namespace k2d
{
    namespace
    {
        const unsigned int kUnknown = ~0u;
    }

    RenderState::RenderState()
    {
        Invalidate();
    }

    void RenderState::Invalidate()
    {
        mProgram = kUnknown;
        mVertexArray = kUnknown;
        mArrayBuffer = kUnknown;
        mElementArrayBuffer = kUnknown;
        mActiveTextureUnit = kUnknown;
        for (unsigned int i = 0; i < kTextureUnits; ++i)
            mTextures[i] = kUnknown;
        mBlendEquation = kUnknown;
        mBlendSource = kUnknown;
        mBlendDestination = kUnknown;
        mBlendEnabled = -1;
        mDepthTestEnabled = -1;
        mDepthWriteEnabled = -1;
        mCullFaceEnabled = -1;
    }

    void RenderState::UseProgram(unsigned int program)
    {
        if (mProgram == program)
            return;
        glUseProgram(program);
        mProgram = program;
    }

    void RenderState::BindVertexArray(unsigned int vao)
    {
        if (mVertexArray == vao)
            return;
        glBindVertexArray(vao);
        mVertexArray = vao;
    }

    void RenderState::BindArrayBuffer(unsigned int buffer)
    {
        if (mArrayBuffer == buffer)
            return;
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        mArrayBuffer = buffer;
    }

    void RenderState::BindElementArrayBuffer(unsigned int buffer)
    {
        if (mElementArrayBuffer == buffer)
            return;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
        mElementArrayBuffer = buffer;
    }

    void RenderState::BindTexture2D(unsigned int unit, unsigned int texture)
    {
        if (unit >= kTextureUnits)
            return;
        if (mActiveTextureUnit != unit)
        {
            glActiveTexture(GL_TEXTURE0 + unit);
            mActiveTextureUnit = unit;
        }
        if (mTextures[unit] == texture)
            return;
        glBindTexture(GL_TEXTURE_2D, texture);
        mTextures[unit] = texture;
    }

    void RenderState::SetBlendEnabled(bool enabled)
    {
        const int value = enabled ? 1 : 0;
        if (mBlendEnabled == value)
            return;
        if (enabled)
            glEnable(GL_BLEND);
        else
            glDisable(GL_BLEND);
        mBlendEnabled = value;
    }

    void RenderState::SetBlendEquation(unsigned int equation)
    {
        if (mBlendEquation == equation)
            return;
        glBlendEquation(equation);
        mBlendEquation = equation;
    }

    void RenderState::SetBlendFunc(unsigned int source, unsigned int destination)
    {
        if (mBlendSource == source && mBlendDestination == destination)
            return;
        glBlendFunc(source, destination);
        mBlendSource = source;
        mBlendDestination = destination;
    }

    void RenderState::SetDepthTestEnabled(bool enabled)
    {
        const int value = enabled ? 1 : 0;
        if (mDepthTestEnabled == value)
            return;
        if (enabled)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);
        mDepthTestEnabled = value;
    }

    void RenderState::SetDepthWriteEnabled(bool enabled)
    {
        const int value = enabled ? 1 : 0;
        if (mDepthWriteEnabled == value)
            return;
        glDepthMask(enabled ? GL_TRUE : GL_FALSE);
        mDepthWriteEnabled = value;
    }

    void RenderState::SetCullFaceEnabled(bool enabled)
    {
        const int value = enabled ? 1 : 0;
        if (mCullFaceEnabled == value)
            return;
        if (enabled)
            glEnable(GL_CULL_FACE);
        else
            glDisable(GL_CULL_FACE);
        mCullFaceEnabled = value;
    }
}
