#include "k2d/RectShape.h"

#include "k2d/GameObject.h"
#include "k2d/RenderQueue.h"

#include <cmath>

namespace k2d
{

namespace
{
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

RectShape::RectShape()
    : Component(Type, ComponentEventRender), mTriangles(), mSize(64.0f, 64.0f),
      mMode(ShapeRenderMode::Fill), mLineWidth(2.0f), mColor(0xFFFFFFFFu), mBlendMode(BLEND_MIX)
{
    rebuild();
}

void RectShape::setSize(const Math::Vec2 &size)
{
    mSize = Math::Vec2(std::fabs(size.x), std::fabs(size.y));
    rebuild();
}

void RectShape::setMode(ShapeRenderMode mode)
{
    mMode = mode;
    rebuild();
}

void RectShape::setLineWidth(float width)
{
    mLineWidth = width > 0.0f ? width : 0.01f;
    rebuild();
}

void RectShape::setColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    mColor = (unsigned int)r | ((unsigned int)g << 8) |
             ((unsigned int)b << 16) | ((unsigned int)a << 24);
}

void RectShape::rebuild()
{
    mTriangles.clear();
    if (mSize.x <= 0.0f || mSize.y <= 0.0f)
        return;

    const float halfWidth = mSize.x * 0.5f;
    const float halfHeight = mSize.y * 0.5f;
    const Math::Vec2 corners[4] = {
        Math::Vec2(-halfWidth, -halfHeight), Math::Vec2(halfWidth, -halfHeight),
        Math::Vec2(halfWidth, halfHeight), Math::Vec2(-halfWidth, halfHeight)};

    if (mMode == ShapeRenderMode::Fill)
    {
        mTriangles.push_back(corners[0]);
        mTriangles.push_back(corners[1]);
        mTriangles.push_back(corners[2]);
        mTriangles.push_back(corners[0]);
        mTriangles.push_back(corners[2]);
        mTriangles.push_back(corners[3]);
        return;
    }

    for (int i = 0; i < 4; ++i)
        addSegment(mTriangles, corners[i], corners[(i + 1) % 4], mLineWidth);
}

void RectShape::onRender(RenderQueue &queue)
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
