#pragma once

#include <glad/glad.h>

namespace k2d
{
    // Per-renderer OpenGL state cache. Call Invalidate() whenever code outside
    // the cache changes GL state (for example ImGui or a custom render pass).
    class RenderState
    {
    public:
        RenderState();

        void Invalidate();
        void UseProgram(unsigned int program);
        void BindVertexArray(unsigned int vao);
        void BindArrayBuffer(unsigned int buffer);
        void BindElementArrayBuffer(unsigned int buffer);
        void BindTexture2D(unsigned int unit, unsigned int texture);

        void SetBlendEnabled(bool enabled);
        void SetBlendEquation(unsigned int equation);
        void SetBlendFunc(unsigned int source, unsigned int destination);
        void SetDepthTestEnabled(bool enabled);
        void SetDepthWriteEnabled(bool enabled);
        void SetCullFaceEnabled(bool enabled);

    private:
        static const unsigned int kTextureUnits = 8;
        unsigned int mProgram;
        unsigned int mVertexArray;
        unsigned int mArrayBuffer;
        unsigned int mElementArrayBuffer;
        unsigned int mActiveTextureUnit;
        unsigned int mTextures[kTextureUnits];
        unsigned int mBlendEquation;
        unsigned int mBlendSource;
        unsigned int mBlendDestination;
        // -1 means unknown after Invalidate().
        int mBlendEnabled;
        int mDepthTestEnabled;
        int mDepthWriteEnabled;
        int mCullFaceEnabled;
    };
}
