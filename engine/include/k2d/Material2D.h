#pragma once

#include "k2d/CanvasTypes.h"

#include <glm/glm.hpp>

namespace k2d
{

    class Texture;

    class Material2D
    {
    public:
        Material2D();

        Texture *texture() const { return mTexture; }
        void setTexture(Texture *texture) { mTexture = texture; }
        const glm::vec4 &color() const { return mColor; }
        void setColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255);
        const glm::vec4 &sourceRect() const { return mSourceRect; }
        bool hasSourceRect() const { return mHasSourceRect; }
        void setSourceRect(float x, float y, float width, float height);
        void clearSourceRect();
        const glm::vec2 &pivot() const { return mPivot; }
        void setPivot(const glm::vec2 &pivot) { mPivot = pivot; }
        bool flipX() const { return mFlipX; }
        bool flipY() const { return mFlipY; }
        void setFlip(bool flipX, bool flipY) { mFlipX = flipX; mFlipY = flipY; }
        BlendMode blendMode() const { return mBlendMode; }
        void setBlendMode(BlendMode mode) { mBlendMode = mode; }

    private:
        Texture *mTexture;
        glm::vec4 mColor;
        glm::vec4 mSourceRect;
        glm::vec2 mPivot;
        BlendMode mBlendMode;
        bool mHasSourceRect;
        bool mFlipX;
        bool mFlipY;
    };

}
