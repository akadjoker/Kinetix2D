#include "k2d/Wander2D.h"

#include <cmath>

#include "k2d/GameObject.h"
#include "k2d/Utils.h"

namespace k2d
{
namespace
{
constexpr float kTwoPi = 6.28318530717958647692f;
}


namespace
{
// Every wanderer needs its own stream, or a crowd of them turns in lockstep.
// Object and component ids are assigned deterministically, so the walk replays
// identically for a given scene.
uint32_t seedFor(const GameObject *object, uint32_t componentId)
{
    const uint32_t objectId = object ? static_cast<uint32_t>(object->id()) : 1u;
    return ((objectId * 2654435761u) ^ (componentId * 40503u)) | 1u;
}

float nextSigned(uint32_t &state)
{
    state = state * 1664525u + 1013904223u;
    return static_cast<float>(state >> 8) * (2.0f / 16777216.0f) - 1.0f;
}
}

Wander2D::Wander2D() : mRadius(24.0f), mDistance(48.0f), mJitter(6.0f), mAngle(0.0f), mRandom(0u)
{
}

void Wander2D::setRadius(float radius)
{
    mRadius = Max(0.0f, radius);
}

void Wander2D::setDistance(float distance)
{
    mDistance = Max(0.0f, distance);
}

void Wander2D::setJitter(float jitter)
{
    mJitter = Max(0.0f, jitter);
}

Math::Vec2 Wander2D::force(float deltaTime, const Math::Vec2 &, const Math::Vec2 &velocity) const
{
    if (mRandom == 0u)
        mRandom = seedFor(owner(), id());

    // mAngle is the only persistent float in the steering set: one non-finite
    // deltaTime would poison it for the rest of the session, so reject the step
    // rather than accumulate it, and keep the angle bounded.
    const float step = nextSigned(mRandom) * mJitter * deltaTime;
    if (std::isfinite(step))
        mAngle += step;
    if (!std::isfinite(mAngle))
        mAngle = 0.0f;
    if (mAngle > kTwoPi || mAngle < -kTwoPi)
        mAngle = std::fmod(mAngle, kTwoPi);

    // Falling back to right() when the agent is at rest pins the wander to one
    // fixed axis forever, so drift the fallback with the angle instead.
    Math::Vec2 heading = velocity;
    if (!(heading.LengthSquared() > kSteeringEpsilon * kSteeringEpsilon))
        heading = Math::Vec2::FromAngle(mAngle);
    heading = heading.NormalizedSafe();

    const Math::Vec2 offset = heading * mDistance + Math::Vec2::FromAngle(mAngle) * mRadius;
    if (!(offset.LengthSquared() > kSteeringEpsilon * kSteeringEpsilon))
        return heading;
    return offset.NormalizedSafe();
}

}
