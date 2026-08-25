#pragma once

#include "k2d/Component.h"
#include "k2d/CanvasTypes.h"
#include "k2d/Color.h"
#include "k2d/Shape2D.h"

#include <ct/vector.hpp>
#include <mathc.h>

namespace k2d
{
    // A visual capsule: a rectangle capped by two semicircles. The longest
    // side determines its orientation automatically.
    class CapsuleShape final : public Component
    {
    public:
        static const ComponentType Type = ComponentType::CapsuleShape;

        CapsuleShape();

        const Math::Vec2 &size() const { return mSize; }
        void setSize(const Math::Vec2 &size);
        int segments() const { return mSegments; }
        void setSegments(int segments);
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
        int mSegments;
        ShapeRenderMode mMode;
        float mLineWidth;
        Color mColor;
        BlendMode mBlendMode;
    };
}
