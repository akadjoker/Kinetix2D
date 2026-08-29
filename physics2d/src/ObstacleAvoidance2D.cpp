#include "k2d/ObstacleAvoidance2D.h"

#include "k2d/Contact2D.h"
#include "k2d/GameObject.h"
#include "k2d/RigidBody2D.h"
#include "k2d/Scene.h"
#include "k2d/Utils.h"

namespace k2d
{

ObstacleAvoidance2D::ObstacleAvoidance2D() : mLookAhead(3.0f), mMask(0xFFFF)
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

// Port of OpenSteer's steerToAvoidObstacles (Obstacle.cpp, Craig Reynolds).
// It sweeps the agent's own body along its forward axis - OpenSteer inflates
// the obstacle by the vehicle radius, which for arbitrary colliders is the
// same thing as casting the collider itself - and on a hit returns the lateral
// component of the surface normal at full force. The force is deliberately not
// scaled by distance: OpenSteer's steerToAvoidIfNeeded returns maxForce or
// nothing, and a ramp is what makes an agent drift into the corner it is
// supposed to be turning away from.
Math::Vec2 ObstacleAvoidance2D::force(float, const Math::Vec2 &position, const Math::Vec2 &velocity) const
{
    GameObject *object = owner();
    Scene *scene = object ? object->scene() : nullptr;
    const float speed = velocity.Length();
    if (!scene || !(speed > kSteeringEpsilon) || mLookAhead <= 0.0f)
        return Math::Vec2(0.0f, 0.0f);

    const Math::Vec2 heading = velocity * (1.0f / speed);
    const Math::Vec2 side(-heading.y, heading.x);
    const float reach = speed * mLookAhead;

    Math::Vec2 normal(0.0f, 0.0f);
    RigidBody2D *body = object->getComponent<RigidBody2D>();
    if (body && body->inWorld() && body->ShapeCount() > 0)
    {
        MotionResult result;
        if (!scene->testMotion(*body, heading * reach, result, 0.0f) || !result.hit)
            return Math::Vec2(0.0f, 0.0f);
        if (result.body && result.shapeIndexOther >= 0 &&
            (result.body->Shapes()[(size_t)result.shapeIndexOther].filter.category & mMask) == 0)
            return Math::Vec2(0.0f, 0.0f);
        normal = result.normal;
    }
    else
    {
        Math::Vec2 point(0.0f, 0.0f);
        if (!scene->raycast(position, heading, reach, &point, &normal, object, mMask))
            return Math::Vec2(0.0f, 0.0f);
    }

    Math::Vec2 lateral = normal - heading * normal.Dot(heading);
    if (!(lateral.LengthSquared() > kSteeringEpsilon * kSteeringEpsilon))
        lateral = side;
    return lateral.NormalizedSafe();
}

}
