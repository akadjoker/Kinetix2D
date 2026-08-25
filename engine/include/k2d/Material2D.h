#pragma once

#include "k2d/CanvasTypes.h"
#include "k2d/Color.h"

#include <mathc.h>

namespace k2d
{

class Texture;

class Material2D
{
  public:
    Material2D();

    Texture* texture() const
    {
        return mTexture;
    }
    void setTexture(Texture* texture)
    {
        mTexture = texture;
    }
    const Color& color() const
    {
        return mColor;
    }
    void setColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255);
    const Math::Vec4& sourceRect() const
    {
        return mSourceRect;
    }
    bool hasSourceRect() const
    {
        return mHasSourceRect;
    }
    void setSourceRect(float x, float y, float width, float height);
    void clearSourceRect();

    const Math::Vec2& tiling() const
    {
        return mTiling;
    }
    void setTiling(float tileX, float tileY)
    {
        mTiling = Math::Vec2(tileX, tileY);
    }

    unsigned int lightMask() const
    {
        return mLightMask;
    }
    void setLightMask(unsigned int mask)
    {
        mLightMask = mask;
    }

    Texture* normalMap() const
    {
        return mNormalMap;
    }
    void setNormalMap(Texture* normalMap)
    {
        mNormalMap = normalMap;
    }

    unsigned int customShader() const
    {
        return mCustomShader;
    }
    void setCustomShader(unsigned int program)
    {
        mCustomShader = program;
    }
    const WaterEffect& water() const
    {
        return mWater;
    }
    WaterEffect& water()
    {
        return mWater;
    }
    bool waterEnabled() const
    {
        return mWater.enabled;
    }
    void setWaterEnabled(bool enabled)
    {
        mWater.enabled = enabled;
    }
    const Math::Vec2& pivot() const
    {
        return mPivot;
    }
    void setPivot(const Math::Vec2& pivot)
    {
        mPivot = pivot;
    }
    bool flipX() const
    {
        return mFlipX;
    }
    bool flipY() const
    {
        return mFlipY;
    }
    void setFlip(bool flipX, bool flipY)
    {
        mFlipX = flipX;
        mFlipY = flipY;
    }
    BlendMode blendMode() const
    {
        return mBlendMode;
    }
    void setBlendMode(BlendMode mode)
    {
        mBlendMode = mode;
    }

  private:
    Texture* mTexture;
    Color mColor;
    Math::Vec4 mSourceRect;
    Math::Vec2 mTiling;
    Math::Vec2 mPivot;
    BlendMode mBlendMode;
    bool mHasSourceRect;
    bool mFlipX;
    bool mFlipY;
    unsigned int mLightMask;
    Texture* mNormalMap;
    unsigned int mCustomShader;
    WaterEffect mWater;
};

} // namespace k2d
