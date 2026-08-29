#include "k2d/ObstacleAvoidance2D.h"

#include "k2d/GameObject.h"
#include "k2d/Scene.h"
#include "k2d/Utils.h"

namespace k2d
{

ObstacleAvoidance2D::ObstacleAvoidance2D() : mLookAhead(0.5f), mMask(0xFFFF)
{
}

void ObstacleAvoidance2D::setLookAhead(float seconds)
{
    mLookAhead = Max(0.0f, seconds);
}

void ObstacleAvoidance2D::setMask(uint16_t mask)
{
    mMask = mask;
}

Math::Vec2 ObstacleAvoidance2D::force(float, const Math::Vec2 &position, const Math::Vec2 &velocity) const
{
    GameObject *object = owner();
    Scene *scene = object ? object->scene() : nullptr;
    const float speed = velocity.Length();
    if (!scene || !(speed > kSteeringEpsilon) || mLookAhead <= 0.0f)
        return Math::Vec2(0.0f, 0.0f);

    const Math::Vec2 heading = velocity * (1.0f / speed);
    const float reach = speed * mLookAhead;

    Math::Vec2 point(0.0f, 0.0f);
    Math::Vec2 normal(0.0f, 0.0f);
    if (!scene->raycast(position, heading, reach, &point, &normal, object, mMask))
        return Math::Vec2(0.0f, 0.0f);

    // A head-on hit leaves no lateral component, and an obstacle centred on
    // the agent leaves no normal at all: both still have to turn somewhere, so
    // fall back on the agent's own left instead of on no force.
    Math::Vec2 lateral = normal - heading * normal.Dot(heading);
    if (!(lateral.LengthSquared() > kSteeringEpsilon * kSteeringEpsilon))
        lateral = Math::Vec2(-heading.y, heading.x);
    lateral = lateral.NormalizedSafe();

    const float travelled = (point - position).Length();
    const float urgency = travelled < reach ? 1.0f - travelled / reach : 0.0f;
    return lateral * (urgency > kSteeringEpsilon ? urgency : kSteeringEpsilon);
}

}
