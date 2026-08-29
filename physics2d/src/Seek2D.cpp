#include "k2d/Seek2D.h"

#include "k2d/Geometry2D.h"

namespace k2d
{

Seek2D::Seek2D()
{
}

Math::Vec2 Seek2D::force(float, const Math::Vec2 &position, const Math::Vec2 &) const
{
    Math::Vec2 point(0.0f, 0.0f);
    if (!target(point))
        return Math::Vec2(0.0f, 0.0f);

    const Math::Vec2 delta = point - position;
    if (!(delta.LengthSquared() > kSteeringEpsilon * kSteeringEpsilon))
        return Math::Vec2(0.0f, 0.0f);
    return Normalize(delta);
}

}
