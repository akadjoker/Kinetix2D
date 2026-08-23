#pragma once

#include "k2d/Component.h"
#include "k2d/CanvasTypes.h"
#include "k2d/Color.h"

#include <mathc.h>

namespace k2d
{

    class Texture;

    class NinePatchComponent : public Component
    {
    public:
        static const ComponentType Type = ComponentType::NinePatch;

        NinePatchComponent();

        void setTexture(Texture *texture) { mTexture = texture; }
        Texture *texture() const { return mTexture; }
        void setSize(const Math::Vec2 &size) { mSize = size; }
        const Math::Vec2 &size() const { return mSize; }

        void setMargins(float left, float top, float right, float bottom);

        Math::Vec4 margins() const { return Math::Vec4(mMarginLeft, mMarginTop, mMarginRight, mMarginBottom); }
        void setPivot(const Math::Vec2 &pivot) { mPivot = pivot; }
        const Math::Vec2 &pivot() const { return mPivot; }
        void setColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255);
        const Color &color() const { return mColor; }
        void setBlendMode(BlendMode mode) { mBlendMode = mode; }
        BlendMode blendMode() const { return mBlendMode; }

    protected:
        void onRender(RenderQueue &queue) override;

    private:
        Texture *mTexture;
        Math::Vec2 mSize;
        float mMarginLeft, mMarginTop, mMarginRight, mMarginBottom;
        Math::Vec2 mPivot;
        Color mColor;
        BlendMode mBlendMode;
    };

}