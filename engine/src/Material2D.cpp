#include "k2d/Material2D.h"

namespace k2d
{

    Material2D::Material2D()
        : mTexture(nullptr), mColor(1.0f), mSourceRect(0.0f), mTiling(1.0f, 1.0f), mPivot(0.5f),
          mBlendMode(BLEND_MIX), mHasSourceRect(false), mFlipX(false), mFlipY(false), mLightMask(1),
          mNormalMap(nullptr), mCustomShader(0)
    {
    }

    void Material2D::setColor(unsigned char r, unsigned char g,
                              unsigned char b, unsigned char a)
    {
        mColor = Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    }

    void Material2D::setSourceRect(float x, float y, float width, float height)
    {
        mSourceRect = Math::Vec4(x, y, width, height);
        mHasSourceRect = width != 0.0f && height != 0.0f;
    }

    void Material2D::clearSourceRect()
    {
        mSourceRect = Math::Vec4(0.0f);
        mHasSourceRect = false;
    }

}