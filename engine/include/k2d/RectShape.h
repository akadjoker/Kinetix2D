#pragma once

#include "k2d/Component.h"
#include "k2d/CanvasTypes.h"
#include "k2d/Color.h"
#include "k2d/Shape2D.h"

#include <ct/vector.hpp>

namespace k2d
{

    class RectShape final : public Component
    {
    public:
        static const ComponentType Type = ComponentType::RectShape;

        RectShape();

        const Math::Vec2 &size() const { return mSize; }
        void setSize(const Math::Vec2 &size);
        ShapeRenderMode mode() const { return mMode; }
        void setMode(ShapeRenderMode mode);
        float lineWidth() const { return mLineWidth; }
        void setLineWidth(float width);
        void setColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255);
        const Color &color() const { return mColor; }
        void setBlendMode(BlendMode mode) { mBlendMode = mode; }
        BlendMode blendMode() const { return mBlendMode; }
        const ct::Vector<Math::Vec2> &triangles() const { return mTriangles; }
        bool valid() const { return !mTriangles.empty(); }

    protected:
        void onRender(RenderQueue &queue) override;

    private:
        void rebuild();

        ct::Vector<Math::Vec2> mTriangles;
        Math::Vec2 mSize;
        ShapeRenderMode mMode;
        float mLineWidth;
        Color mColor;
        BlendMode mBlendMode;
    };

}
