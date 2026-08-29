#include "k2d/Steering2D.h"

#include "k2d/Contact2D.h"
#include "k2d/GameObject.h"
#include "k2d/RigidBody2D.h"
#include "k2d/Scene.h"
#include "k2d/Utils.h"

#include <cmath>
#include <limits>

namespace k2d
{

namespace
{
constexpr uint32_t kUnresolved = (std::numeric_limits<uint32_t>::max)();
constexpr float kTwoPi = 6.28318530717958647692f;

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
} // namespace

Steering2D::Steering2D()
    : Component(Type, ComponentEventNone), mTargetPosition(0.0f, 0.0f), mTargetObject(nullptr),
      mResolvedVersion(kUnresolved), mRandom(0u), mWanderAngle(0.0f), mWeight(1.0f), mSeparationRadius(48.0f),
      mLookAhead(3.0f), mSlowRadius(120.0f), mWanderRadius(24.0f), mWanderDistance(48.0f), mWanderJitter(6.0f),
      mMask(0xFFFF), mSeparationEnabled(false), mAvoidanceEnabled(false)
{
}

Steering2D::~Steering2D()
{
}

void Steering2D::setWeight(float weight)
{
    mWeight = weight;
}

void Steering2D::setSeparationRadius(float radius)
{
    mSeparationRadius = Max(0.0f, radius);
}

void Steering2D::setLookAhead(float seconds)
{
    mLookAhead = Max(0.0f, seconds);
}

void Steering2D::setSlowRadius(float radius)
{
    mSlowRadius = Max(0.0f, radius);
}

void Steering2D::setWanderRadius(float radius)
{
    mWanderRadius = Max(0.0f, radius);
}

void Steering2D::setWanderDistance(float distance)
{
    mWanderDistance = Max(0.0f, distance);
}

void Steering2D::setWanderJitter(float jitter)
{
    mWanderJitter = Max(0.0f, jitter);
}

void Steering2D::setMask(uint16_t mask)
{
    mMask = mask;
}

void Steering2D::setGroupTag(const char *tag)
{
    mGroupTag = tag ? tag : "";
}

void Steering2D::setTargetName(const char *name)
{
    mTargetName = name ? name : "";
    mTargetObject = nullptr;
    mResolvedVersion = kUnresolved;
}

void Steering2D::resolve() const
{
    const GameObject *object = owner();
    const Scene *scene = object ? object->scene() : nullptr;
    if (!scene || mTargetName.empty() || mResolvedVersion == scene->topologyVersion())
        return;

    mTargetObject = scene->find(mTargetName.c_str());
    mResolvedVersion = scene->topologyVersion();
}

bool Steering2D::target(Math::Vec2 &out) const
{
    if (mTargetName.empty())
    {
        out = mTargetPosition;
        return true;
    }

    resolve();
    const GameObject *object = owner();
    const Scene *scene = object ? object->scene() : nullptr;
    if (!mTargetObject || !scene || mResolvedVersion != scene->topologyVersion())
        return false;
    out = mTargetObject->globalPosition();
    return true;
}

Math::Vec2 Steering2D::seek(const Math::Vec2 &target, const Math::Vec2 &velocity, float maxSpeed) const
{
    const GameObject *object = owner();
    if (!object)
        return Math::Vec2(0.0f, 0.0f);
    const Math::Vec2 offset = target - object->globalPosition();
    if (!(offset.LengthSquared() > kSteeringEpsilon * kSteeringEpsilon))
        return Math::Vec2(0.0f, 0.0f);
    return offset.NormalizedSafe() * maxSpeed - velocity;
}

Math::Vec2 Steering2D::flee(const Math::Vec2 &target, const Math::Vec2 &velocity, float maxSpeed) const
{
    const GameObject *object = owner();
    if (!object)
        return Math::Vec2(0.0f, 0.0f);
    const Math::Vec2 offset = object->globalPosition() - target;
    if (!(offset.LengthSquared() > kSteeringEpsilon * kSteeringEpsilon))
        return Math::Vec2(0.0f, 0.0f);
    return offset.NormalizedSafe() * maxSpeed - velocity;
}

Math::Vec2 Steering2D::arrive(const Math::Vec2 &target, const Math::Vec2 &velocity, float maxSpeed) const
{
    const GameObject *object = owner();
    if (!object)
        return Math::Vec2(0.0f, 0.0f);
    const Math::Vec2 offset = target - object->globalPosition();
    const float distance = offset.Length();
    if (!(distance > kSteeringEpsilon))
        return -velocity;

    // OpenSteer's steerForArrival: the wanted speed ramps down inside the
    // slowing distance, so the agent stops on the mark instead of orbiting it.
    const float ramped = mSlowRadius > kSteeringEpsilon ? maxSpeed * (distance / mSlowRadius) : maxSpeed;
    const float clipped = Min(ramped, maxSpeed);
    return offset * (clipped / distance) - velocity;
}

Math::Vec2 Steering2D::wander(float deltaTime, const Math::Vec2 &velocity, float maxSpeed) const
{
    if (mRandom == 0u)
        mRandom = seedFor(owner(), id());

    // mWanderAngle is the only persistent float here: one non-finite deltaTime
    // would poison it for the rest of the session, so reject the step rather
    // than accumulate it, and keep the angle bounded.
    const float step = nextSigned(mRandom) * mWanderJitter * deltaTime;
    if (std::isfinite(step))
        mWanderAngle += step;
    if (!std::isfinite(mWanderAngle))
        mWanderAngle = 0.0f;
    if (mWanderAngle > kTwoPi || mWanderAngle < -kTwoPi)
        mWanderAngle = std::fmod(mWanderAngle, kTwoPi);

    // Falling back to a fixed axis when the agent is at rest pins the wander to
    // it forever, so drift the fallback with the angle instead.
    Math::Vec2 heading = velocity;
    if (!(heading.LengthSquared() > kSteeringEpsilon * kSteeringEpsilon))
        heading = Math::Vec2::FromAngle(mWanderAngle);
    heading = heading.NormalizedSafe();

    const Math::Vec2 offset = heading * mWanderDistance + Math::Vec2::FromAngle(mWanderAngle) * mWanderRadius;
    if (!(offset.LengthSquared() > kSteeringEpsilon * kSteeringEpsilon))
        return heading * maxSpeed;
    return offset.NormalizedSafe() * maxSpeed;
}

Math::Vec2 Steering2D::separation(const Math::Vec2 &, float maxSpeed) const
{
    const GameObject *object = owner();
    Scene *scene = object ? object->scene() : nullptr;
    if (!scene || mSeparationRadius <= 0.0f)
        return Math::Vec2(0.0f, 0.0f);

    const Math::Vec2 position = object->globalPosition();
    Math::Vec2 push(0.0f, 0.0f);
    const std::size_t count = scene->queryNeighbours(*object, position, mSeparationRadius, mMask);
    for (std::size_t i = 0; i < count; ++i)
    {
        const GameObject *neighbour = scene->neighbourAt(i);
        if (!neighbour || !neighbour->isActiveInHierarchy())
            continue;
        // A group filter, the way the Game Institute demo only ever looks at
        // VisibleGroupMembers: without it a crowd shoves the scenery as well.
        if (!mGroupTag.empty() && neighbour->tag() != mGroupTag)
            continue;

        const Math::Vec2 away = position - neighbour->globalPosition();
        const float distance = away.Length();
        if (!(distance < mSeparationRadius))
            continue;

        // OpenSteer steerForSeparation: away from each neighbour with a 1/d
        // falloff, summed, then normalised to a pure direction. Returning the
        // raw sum instead let the force fade to nothing at any real spacing,
        // so a crowd never actually separated.
        if (!(distance > kSteeringEpsilon))
        {
            push += Math::Vec2(object->id() < neighbour->id() ? -1.0f : 1.0f, 0.0f);
            continue;
        }
        push += away * (1.0f / (distance * distance));
    }

    const float strength = push.Length();
    if (!(strength > kSteeringEpsilon))
        return Math::Vec2(0.0f, 0.0f);
    return push * (maxSpeed / strength);
}

float Steering2D::probeOffset() const
{
    const GameObject *object = owner();
    const RigidBody2D *body = object ? object->getComponent<RigidBody2D>() : nullptr;
    if (!body || body->ShapeCount() == 0)
        return 8.0f;

    const AABB &bounds = body->TightAABB();
    const float halfWidth = 0.5f * (bounds.upperBound.x - bounds.lowerBound.x);
    const float halfHeight = 0.5f * (bounds.upperBound.y - bounds.lowerBound.y);
    const float half = Max(halfWidth, halfHeight);
    return half > 1.0f ? half : 8.0f;
}

// Port of OpenSteer's steerToAvoidObstacles. It sweeps the agent's own body
// along its forward axis - OpenSteer inflates the obstacle by the vehicle
// radius, which for arbitrary colliders is the same thing as casting the
// collider itself - and on a hit returns the lateral component of the surface
// normal at full force. The force is deliberately not scaled by distance:
// steerToAvoidIfNeeded returns maxForce or nothing, and a ramp is what makes an
// agent drift into the corner it is supposed to be turning away from.
Math::Vec2 Steering2D::avoidance(const Math::Vec2 &velocity, float maxSpeed) const
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
        if (!scene->raycast(object->globalPosition(), heading, reach, &point, &normal, object, mMask))
            return Math::Vec2(0.0f, 0.0f);
    }

    Math::Vec2 lateral = normal - heading * normal.Dot(heading);
    if (!(lateral.LengthSquared() > kSteeringEpsilon * kSteeringEpsilon))
        lateral = side;
    (void)probeOffset();
    return lateral.NormalizedSafe() * maxSpeed;
}

Math::Vec2 Steering2D::force(float, const Math::Vec2 &, const Math::Vec2 &velocity, bool &outVetoed) const
{
    outVetoed = false;
    if (mAvoidanceEnabled)
    {
        const Math::Vec2 dodge = avoidance(velocity, 1.0f);
        if (dodge.x != 0.0f || dodge.y != 0.0f)
        {
            outVetoed = true;
            return dodge * mWeight;
        }
    }
    if (!mSeparationEnabled)
        return Math::Vec2(0.0f, 0.0f);
    return separation(velocity, 1.0f) * mWeight;
}

}
