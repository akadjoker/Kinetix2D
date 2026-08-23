#pragma once

#include "k2d/Component.h"
#include "k2d/CanvasTypes.h"
#include "k2d/Color.h"

#include <ct/vector.hpp>
#include <mathc.h>

namespace k2d
{

    class Texture;

    class Line2D : public Component
    {
    public:
        static const ComponentType Type = ComponentType::LinePath;

        Line2D();

        void setPoints(const Math::Vec2 *points, int count);
        const ct::Vector<Math::Vec2> &points() const { return mPoints; }
        void setWidth(float width);
        float width() const { return mWidth; }
        void setColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255);
        const Color &color() const { return mColor; }
        void setTexture(Texture *texture) { mTexture = texture; }
        Texture *texture() const { return mTexture; }
        void setClosed(bool closed);
        bool closed() const { return mClosed; }
        void setBlendMode(BlendMode mode) { mBlendMode = mode; }
        BlendMode blendMode() const { return mBlendMode; }

    protected:
        void onRender(RenderQueue &queue) override;

    private:
        void rebuild();

        ct::Vector<Math::Vec2> mPoints;
        ct::Vector<Math::Vec2> mTriangles;
        float mWidth;
        Color mColor;
        Texture *mTexture;
        bool mClosed;
        BlendMode mBlendMode;
    };

}