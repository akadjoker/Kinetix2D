#include "k2d/CircleShape.h"

#include "k2d/GameObject.h"
#include "k2d/RenderQueue.h"

#include <cmath>

namespace k2d
{

namespace
{
constexpr float kPi = 3.14159265358979323846f;

void addSegment(ct::Vector<Math::Vec2> &triangles, const Math::Vec2 &a, const Math::Vec2 &b,
                float width)
{
    Math::Vec2 direction = b - a;
    const float length = direction.Length();
    if (length < 0.0001f)
        return;
    direction /= length;
    const Math::Vec2 normal(-direction.y * width * 0.5f, direction.x * width * 0.5f);

    triangles.push_back(a - normal);
    triangles.push_back(a + normal);
    triangles.push_back(b + normal);
    triangles.push_back(a - normal);
    triangles.push_back(b + normal);
    triangles.push_back(b - normal);
}
}

CircleShape::CircleShape()
    : Component(Type, ComponentEventRender), mTriangles(), mRadius(32.0f), mSegments(32),
      mMode(ShapeRenderMode::Fill), mLineWidth(2.0f), mColor(0xFFFFFFFFu), mBlendMode(BLEND_MIX)
{
    rebuild();
}

void CircleShape::setRadius(float radius)
{
    mRadius = radius > 0.0f ? radius : 0.0f;
    rebuild();
}

void CircleShape::setSegments(int segments)
{
    mSegments = segments < 3 ? 3 : (segments > 512 ? 512 : segments);
    rebuild();
}

void CircleShape::setMode(ShapeRenderMode mode)
{
    mMode = mode;
    rebuild();
}

void CircleShape::setLineWidth(float width)
{
    mLineWidth = width > 0.0f ? width : 0.01f;
    rebuild();
}

void CircleShape::setColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    mColor = (unsigned int)r | ((unsigned int)g << 8) |
             ((unsigned int)b << 16) | ((unsigned int)a << 24);
}

void CircleShape::rebuild()
{
    mTriangles.clear();
    if (mRadius <= 0.0f)
        return;

    if (mMode == ShapeRenderMode::Fill)
    {
        for (int i = 0; i < mSegments; ++i)
        {
            const float a0 = (float)i * 2.0f * kPi / (float)mSegments;
            const float a1 = (float)(i + 1) * 2.0f * kPi / (float)mSegments;
            mTriangles.push_back(Math::Vec2(0.0f, 0.0f));
            mTriangles.push_back(Math::Vec2(std::cos(a0) * mRadius, std::sin(a0) * mRadius));
            mTriangles.push_back(Math::Vec2(std::cos(a1) * mRadius, std::sin(a1) * mRadius));
        }
        return;
    }

    for (int i = 0; i < mSegments; ++i)
    {
        const float a0 = (float)i * 2.0f * kPi / (float)mSegments;
        const float a1 = (float)(i + 1) * 2.0f * kPi / (float)mSegments;
        addSegment(mTriangles, Math::Vec2(std::cos(a0) * mRadius, std::sin(a0) * mRadius),
                   Math::Vec2(std::cos(a1) * mRadius, std::sin(a1) * mRadius), mLineWidth);
    }
}

void CircleShape::onRender(RenderQueue &queue)
{
    if (mTriangles.empty())
        return;

    RenderItem &item = queue.AddItem(owner()->zIndex());
    item.xform = owner()->globalTransform();
    item.blendMode = mBlendMode;
    RenderCommand command;
    command.type = RenderCommand::kPolygon;
    command.color = mColor;
    command.polygonPoints = &mTriangles;
    command.polygonPointCount = (unsigned int)mTriangles.size();
    item.commands.push_back(command);
}

}
