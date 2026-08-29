#include "k2d/Flee2D.h"

#include "k2d/Geometry2D.h"
#include "k2d/Utils.h"

#include <cmath>

namespace k2d
{

Flee2D::Flee2D() : mRadius(0.0f)
{
}

void Flee2D::setRadius(float radius)
{
    mRadius = Max(0.0f, radius);
}

Math::Vec2 Flee2D::force(float, const Math::Vec2 &position, const Math::Vec2 &) const
{
    Math::Vec2 point(0.0f, 0.0f);
    if (!target(point))
        return Math::Vec2(0.0f, 0.0f);

    const Math::Vec2 away = position - point;
    const float distance = away.Length();
    if (!(distance > kSteeringEpsilon))
        return Math::Vec2(0.0f, 0.0f);
    if (mRadius <= 0.0f)
        return away * (1.0f / distance);
    if (!(distance < mRadius))
        return Math::Vec2(0.0f, 0.0f);
    return away * ((1.0f - distance / mRadius) / distance);
}

}
