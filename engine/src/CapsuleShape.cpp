#include "k2d/CapsuleShape.h"

#include "k2d/GameObject.h"
#include "k2d/RenderQueue.h"

#include <cmath>

namespace k2d
{
namespace
{
constexpr float kPi = 3.14159265358979323846f;

void addSegment(ct::Vector<Math::Vec2> &triangles, const Math::Vec2 &a, const Math::Vec2 &b, float width)
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

CapsuleShape::CapsuleShape()
    : Component(Type, ComponentEventRender), mTriangles(), mSize(128.0f, 64.0f), mSegments(16),
      mMode(ShapeRenderMode::Fill), mLineWidth(2.0f), mColor(0xFFFFFFFFu), mBlendMode(BLEND_MIX)
{
    rebuild();
}

void CapsuleShape::setSize(const Math::Vec2 &size)
{
    mSize = Math::Vec2(std::fabs(size.x), std::fabs(size.y));
    rebuild();
}

void CapsuleShape::setSegments(int segments)
{
    mSegments = segments < 3 ? 3 : (segments > 512 ? 512 : segments);
    rebuild();
}

void CapsuleShape::setMode(ShapeRenderMode mode)
{
    mMode = mode;
    rebuild();
}

void CapsuleShape::setLineWidth(float width)
{
    mLineWidth = width > 0.0f ? width : 0.01f;
    rebuild();
}

void CapsuleShape::setColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    mColor = (unsigned int)r | ((unsigned int)g << 8) |
             ((unsigned int)b << 16) | ((unsigned int)a << 24);
}

void CapsuleShape::rebuild()
{
    mTriangles.clear();
    const float radius = (mSize.x < mSize.y ? mSize.x : mSize.y) * 0.5f;
    if (radius <= 0.0f)
        return;

    ct::Vector<Math::Vec2> edge;
    edge.reserve((size_t)(mSegments + 1) * 2);
    if (mSize.x >= mSize.y)
    {
        const float halfCenterDistance = mSize.x * 0.5f - radius;
        for (int i = 0; i <= mSegments; ++i)
        {
            const float angle = -kPi * 0.5f + kPi * (float)i / (float)mSegments;
            edge.push_back(Math::Vec2(halfCenterDistance + std::cos(angle) * radius, std::sin(angle) * radius));
        }
        for (int i = 0; i <= mSegments; ++i)
        {
            const float angle = kPi * 0.5f + kPi * (float)i / (float)mSegments;
            edge.push_back(Math::Vec2(-halfCenterDistance + std::cos(angle) * radius, std::sin(angle) * radius));
        }
    }
    else
    {
        const float halfCenterDistance = mSize.y * 0.5f - radius;
        for (int i = 0; i <= mSegments; ++i)
        {
            const float angle = kPi * (float)i / (float)mSegments;
            edge.push_back(Math::Vec2(std::cos(angle) * radius, halfCenterDistance + std::sin(angle) * radius));
        }
        for (int i = 0; i <= mSegments; ++i)
        {
            const float angle = kPi + kPi * (float)i / (float)mSegments;
            edge.push_back(Math::Vec2(std::cos(angle) * radius, -halfCenterDistance + std::sin(angle) * radius));
        }
    }

    if (mMode == ShapeRenderMode::Fill)
    {
        for (size_t i = 0; i < edge.size(); ++i)
        {
            mTriangles.push_back(Math::Vec2(0.0f));
            mTriangles.push_back(edge[i]);
            mTriangles.push_back(edge[(i + 1) % edge.size()]);
        }
        return;
    }

    for (size_t i = 0; i < edge.size(); ++i)
        addSegment(mTriangles, edge[i], edge[(i + 1) % edge.size()], mLineWidth);
}

void CapsuleShape::onRender(RenderQueue &queue)
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
