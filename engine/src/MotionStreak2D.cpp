#include "k2d/MotionStreak2D.h"
#include "k2d/GameObject.h"
#include "k2d/RenderQueue.h"
#include <algorithm>
#include <cmath>
namespace k2d
{
MotionStreak2D::MotionStreak2D() : Component(Type, ComponentEventUpdate | ComponentEventRender)
{
}
void MotionStreak2D::reset()
{
    mPoints.clear();
    mInitialized = false;
}
void MotionStreak2D::setLifetime(float v)
{
    mLifetime = std::max(.01f, v);
}
void MotionStreak2D::setWidth(float v)
{
    mWidth = std::max(.01f, v);
}
void MotionStreak2D::setMinDistance(float v)
{
    mMinDistance = std::max(.01f, v);
}
void MotionStreak2D::onUpdate(float dt)
{
    for (Point& p : mPoints)
        p.age += dt;
    size_t expired = 0;
    while (expired < mPoints.size() && mPoints[expired].age >= mLifetime)
        ++expired;
    if (expired > 0)
        mPoints.erase(mPoints.begin(), mPoints.begin() + expired);
    if (!owner())
        return;
    const Math::Vec2 pos = owner()->globalPosition();
    if (!mInitialized)
    {
        mLastPosition = pos;
        mPoints.push_back({pos, 0});
        mInitialized = true;
        return;
    }
    Math::Vec2 d = pos - mLastPosition;
    if (d.x * d.x + d.y * d.y >= mMinDistance * mMinDistance)
    {
        mPoints.push_back({pos, 0});
        mLastPosition = pos;
    }
}
void MotionStreak2D::onRender(RenderQueue& queue)
{
    if (mPoints.size() < 2)
        return;
    RenderItem& item = queue.AddItem(owner()->zIndex());
    item.xform = Matrix2D::Identity();
    item.blendMode = mBlend;
    for (size_t i = 0; i + 1 < mPoints.size(); ++i)
    {
        const Point &a = mPoints[i], &b = mPoints[i + 1];
        Math::Vec2 d = b.position - a.position;
        float len = d.Length();
        if (len < .001f)
            continue;
        Math::Vec2 n(-d.y / len, d.x / len);
        float h = mWidth * .5f;
        RenderCommand c;
        c.type = RenderCommand::kPolygon;
        c.color =
            Color(mColor.r, mColor.g, mColor.b, mColor.a * std::max(0.f, 1.f - (a.age + b.age) * .5f / mLifetime));
        c.ownedPolygonPoints.push_back(a.position - n * h);
        c.ownedPolygonPoints.push_back(a.position + n * h);
        c.ownedPolygonPoints.push_back(b.position + n * h);
        c.ownedPolygonPoints.push_back(a.position - n * h);
        c.ownedPolygonPoints.push_back(b.position + n * h);
        c.ownedPolygonPoints.push_back(b.position - n * h);
        item.commands.push_back(c);
    }
}
} // namespace k2d
