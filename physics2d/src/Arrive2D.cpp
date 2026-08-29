#include "k2d/Arrive2D.h"

#include "k2d/Utils.h"

namespace k2d
{

Arrive2D::Arrive2D() : mSlowRadius(96.0f), mStopRadius(4.0f)
{
}

void Arrive2D::setSlowRadius(float radius)
{
    mSlowRadius = Max(0.0f, radius);
}

void Arrive2D::setStopRadius(float radius)
{
    mStopRadius = Max(0.0f, radius);
}

Math::Vec2 Arrive2D::force(float, const Math::Vec2 &position, const Math::Vec2 &) const
{
    Math::Vec2 point(0.0f, 0.0f);
    if (!target(point))
        return Math::Vec2(0.0f, 0.0f);

    const Math::Vec2 delta = point - position;
    const float distance = delta.Length();
    if (!(distance > kSteeringEpsilon) || distance <= mStopRadius)
        return Math::Vec2(0.0f, 0.0f);

    float scale = 1.0f;
    if (mSlowRadius > mStopRadius && distance < mSlowRadius)
        scale = (distance - mStopRadius) / (mSlowRadius - mStopRadius);
    return delta * (scale / distance);
}

}
