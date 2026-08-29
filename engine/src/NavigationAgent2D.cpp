#include "k2d/NavigationAgent2D.h"
#include "k2d/Utils.h"

#include "k2d/GameObject.h"
#include "k2d/Geometry2D.h"
#include "k2d/Navigation2D.h"
#include "k2d/RigidBody2D.h"
#include "k2d/Scene.h"

#include <cmath>

namespace k2d
{
NavigationAgent2D::NavigationAgent2D() : Component(Type, ComponentEventUpdate)
{
}

bool NavigationAgent2D::setTargetPosition(const Math::Vec2& position)
{
    return retarget(position, false);
}

bool NavigationAgent2D::retarget(const Math::Vec2& position, bool forceRepath)
{
    mTarget = position;
    mHasTarget = true;
    const bool movedEnough = !mHasPathedTarget ||
        DistanceSquared(position, mLastPathedTarget) > mRepathMoveThreshold * mRepathMoveThreshold;
    if (!forceRepath && !movedEnough && hasPath())
        return true;
    const bool ok = repath();
    mLastPathedTarget = mTarget;
    mHasPathedTarget = true;
    return ok;
}

bool NavigationAgent2D::followPosition(Math::Vec2& out) const
{
    // mFollowTarget is only re-resolved inside onUpdate, so a caller reaching
    // here first in the frame after the target was destroyed would read freed
    // memory. Trust the pointer only while the topology it was resolved under
    // still stands.
    const GameObject* object = owner();
    const Scene* scene = object ? object->scene() : nullptr;
    if (!mFollowTarget || !scene || mFollowVersion != scene->topologyVersion())
        return false;
    out = mFollowTarget->globalPosition();
    return true;
}

bool NavigationAgent2D::repath()
{
    ++mRepathCount;
    mPath.clear();
    mPathIndex = 0;
    if (!mHasTarget || !owner() || !owner()->scene())
        return false;
    if (!Navigation2D::GetPath(*owner()->scene(), owner()->globalPosition(), mTarget, mPath))
        return false;
    if (mPath.size() > 1)
        mPathIndex = 1;
    else
        mPathIndex = mPath.size();
    return true;
}

void NavigationAgent2D::clearPath()
{
    mPath.clear();
    mPathIndex = 0;
    mHasTarget = false;
}

Math::Vec2 NavigationAgent2D::nextPathPosition() const
{
    return hasPath() ? mPath[mPathIndex] : (owner() ? owner()->globalPosition() : Math::Vec2(0.0f));
}

void NavigationAgent2D::advance()
{
    if (!owner())
        return;
    while (hasPath())
    {
        const Math::Vec2 delta = nextPathPosition() - owner()->globalPosition();
        if (delta.x * delta.x + delta.y * delta.y > mPathDesiredDistance * mPathDesiredDistance)
            break;
        ++mPathIndex;
    }
}

void NavigationAgent2D::setPathDesiredDistance(float value)
{
    mPathDesiredDistance = Max(0.01f, value);
}

void NavigationAgent2D::setMaxSpeed(float value)
{
    mMaxSpeed = Max(0.0f, value);
}

void NavigationAgent2D::setRotationLerpSpeed(float value)
{
    mRotationLerpSpeed = Max(0.0f, value);
}

void NavigationAgent2D::setRepathInterval(float value)
{
    mRepathInterval = Max(0.0f, value);
}

void NavigationAgent2D::setRepathMoveThreshold(float value)
{
    mRepathMoveThreshold = Max(0.0f, value);
}

void NavigationAgent2D::onAwake()
{
    // Spread the repath phase across the interval. A wave of agents spawned on
    // the same frame otherwise keeps searching on the same frame for as long as
    // it lives, and the whole crowd's pathfinding lands in one spike instead of
    // being smeared over the interval it was given.
    if (const GameObject* object = owner())
    {
        uint32_t state = ((uint32_t)object->id() * 2654435761u) | 1u;
        state = state * 1664525u + 1013904223u;
        mRepathTimer = mRepathInterval * (float)(state >> 8) * (1.0f / 16777216.0f);
    }
    if (mHasTarget)
        retarget(mTarget, true);
}

void NavigationAgent2D::updateFollowTarget(float deltaTime)
{
    if (!hasFollowTarget() || !owner() || !owner()->scene())
        return;
    Scene* scene = owner()->scene();
    // Gate purely on the version, never on mFollowTarget being null: a name
    // that resolves to nothing would otherwise re-walk the whole tree every
    // frame. Steering2D::resolve() has this right; this did not.
    if (mFollowVersion != scene->topologyVersion())
    {
        mFollowTarget = scene->find(mFollowTargetName.c_str());
        mFollowVersion = scene->topologyVersion();
    }
    if (!mFollowTarget)
        return;
    const Math::Vec2 candidate = mFollowTarget->globalPosition();
    // A failed search leaves no path, so retrying on "no path" alone bypasses
    // the interval and runs a full pathfind every frame for as long as the
    // target stays unreachable. The interval governs both cases.
    mRepathTimer += deltaTime;
    if (!hasPath())
    {
        if (mRepathTimer >= mRepathInterval || !mHasPathedTarget)
        {
            retarget(candidate, false);
            mRepathTimer = 0.0f;
        }
        else
        {
            mTarget = candidate;
            mHasTarget = true;
        }
        return;
    }
    if (mRepathTimer >= mRepathInterval)
    {
        retarget(candidate, false);
        mRepathTimer = 0.0f;
        return;
    }
    mTarget = candidate;
    mHasTarget = true;
}

// Paths and steering are global-space, but GameObject::translate adds in the
// parent's frame. Under a rotated or scaled parent the two disagree and the
// agent orbits its waypoint instead of reaching it.
void NavigationAgent2D::translateGlobal(const Math::Vec2& offset)
{
    GameObject* object = owner();
    if (!object)
        return;

    // pushTransforms skips dynamic bodies and pullTransforms writes their own
    // integrated position back over the object, so a translate here would be
    // silently erased. Drive such a body through its velocity instead.
    if (RigidBody2D* body = object->getComponent<RigidBody2D>())
        if (body->bodyType() == BodyType::Dynamic)
        {
            const float dt = mLastDeltaTime;
            if (dt > 0.0001f)
                body->setVelocity(offset * (1.0f / dt));
            return;
        }

    GameObject* parent = object->parent();
    if (!parent || !parent->parent())
    {
        object->translate(offset);
        return;
    }
    const Matrix2D inverse = parent->globalTransform().AffineInverse();
    const Math::Vec2 origin = inverse.Transform(Math::Vec2(0.0f, 0.0f));
    object->translate(inverse.Transform(offset) - origin);
}

void NavigationAgent2D::onUpdate(float deltaTime)
{
    mLastDeltaTime = deltaTime;
    updateFollowTarget(deltaTime);
    advance();
    if (!owner())
        return;

    const bool following = hasPath();
    Math::Vec2 delta(0.0f, 0.0f);
    float length = 0.0f;
    if (following)
    {
        delta = nextPathPosition() - owner()->globalPosition();
        length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        if (length <= 0.0001f)
            return;
        if (mOrientToPath)
        {
            constexpr float kRadiansToDegrees = 57.2957795131f;
            // delta is global; setRotationDegrees writes the local angle, so a
            // rotated parent has to be discounted or the facing is skewed.
            const float parentRotation = owner()->parent() ? owner()->parent()->globalRotationDegrees() : 0.0f;
            const float wanted =
                std::atan2(delta.y, delta.x) * kRadiansToDegrees + mRotationOffsetDegrees - parentRotation;
            const float current = owner()->rotationDegrees();
            float angleDelta = std::fmod(wanted - current + 180.0f, 360.0f);
            if (angleDelta < 0.0f)
                angleDelta += 360.0f;
            angleDelta -= 180.0f;
            const float alpha = mRotationLerpSpeed <= 0.0f ? 1.0f : 1.0f - std::exp(-mRotationLerpSpeed * deltaTime);
            owner()->setRotationDegrees(current + angleDelta * alpha);
        }
    }
    if (!mAutoMove)
        return;

    const Math::Vec2 pathDesire = following ? delta * (mMaxSpeed / length) : Math::Vec2(0.0f, 0.0f);
    Scene *scene = owner()->scene();
    // Steering behaviours are asked what to do about the direction the agent is
    // actually travelling. Handing them the path desire instead left every
    // velocity-driven behaviour - obstacle avoidance above all - reading a zero
    // speed and returning no force whenever the agent had no path.
    const Math::Vec2 sense = mVelocity.LengthSquared() > 0.0001f ? mVelocity : pathDesire;
    bool vetoed = false;
    const Math::Vec2 steering =
        scene ? scene->steeringForce(*owner(), sense, deltaTime, &vetoed) : Math::Vec2(0.0f, 0.0f);

    const Math::Vec2 before = owner()->globalPosition();
    if (!vetoed && steering.x == 0.0f && steering.y == 0.0f)
    {
        if (!following)
        {
            mVelocity = Math::Vec2(0.0f, 0.0f);
            mSmoothedAcceleration = Math::Vec2(0.0f, 0.0f);
            releaseBody();
            return;
        }
        const float distance = Min(length, mMaxSpeed * deltaTime);
        translateGlobal(delta * (distance / length));
        recordVelocity(before, deltaTime);
        advance();
        return;
    }

    // A steering force is an acceleration, never a velocity. Assigning it as a
    // velocity turned a purely lateral avoidance force into purely lateral
    // motion, so the agent lost all forward momentum and thrashed side to side
    // instead of curving around what it was avoiding.
    Math::Vec2 force = steering * mMaxSpeed;
    if (!vetoed && following)
        force += pathDesire - mVelocity;
    applySteeringForce(force, deltaTime);

    const float speed = mVelocity.Length();
    if (speed > 0.0001f)
    {
        const float step = following ? Min(length, speed * deltaTime) : speed * deltaTime;
        translateGlobal(mVelocity * (step / speed));
    }
    advance();
}

// Port of OpenSteer's SimpleVehicle::applySteeringForce, including
// adjustRawSteeringForce, which is what stops a stationary agent from being
// shoved backwards or sideways by a force it has no momentum to absorb.
void NavigationAgent2D::applySteeringForce(Math::Vec2 force, float deltaTime)
{
    if (deltaTime <= 0.0f)
        return;

    const float speed = mVelocity.Length();
    const float maxAdjustedSpeed = 0.2f * mMaxSpeed;
    const float forceLength = force.Length();
    if (speed <= maxAdjustedSpeed && forceLength > 0.0f && speed > 0.0001f)
    {
        const Math::Vec2 forward = mVelocity * (1.0f / speed);
        const float range = maxAdjustedSpeed > 0.0f ? speed / maxAdjustedSpeed : 1.0f;
        const float cone = 1.0f - 2.0f * std::pow(range, 20.0f);
        const Math::Vec2 direction = force * (1.0f / forceLength);
        if (direction.Dot(forward) < cone)
        {
            Math::Vec2 perpendicular = force - forward * force.Dot(forward);
            const float perpendicularLength = perpendicular.Length();
            perpendicular = perpendicularLength > 0.0001f ? perpendicular * (1.0f / perpendicularLength)
                                                          : Math::Vec2(-forward.y, forward.x);
            const float sine = std::sqrt(Max(0.0f, 1.0f - cone * cone));
            force = (forward * cone + perpendicular * sine) * forceLength;
        }
    }

    // OpenSteer's Pedestrian runs maxSpeed 2 against maxForce 8, so a vehicle
    // can change its whole velocity four times a second.
    const float maxForce = mMaxSpeed * 4.0f;
    const float clipped = force.Length();
    if (clipped > maxForce)
        force = force * (maxForce / clipped);

    const float smoothRate = Clamp(9.0f * deltaTime, 0.15f, 0.4f);
    mSmoothedAcceleration += (force - mSmoothedAcceleration) * smoothRate;
    mVelocity += mSmoothedAcceleration * deltaTime;

    const float newSpeed = mVelocity.Length();
    if (newSpeed > mMaxSpeed)
        mVelocity = mVelocity * (mMaxSpeed / newSpeed);
}

// A dynamic body is driven by having its velocity written every frame, so
// arriving has to be said out loud. Falling silent leaves the body coasting on
// the last velocity it was given, which carries it off the spot it just
// reached and straight into being sent back again.
void NavigationAgent2D::releaseBody()
{
    if (!owner())
        return;
    RigidBody2D* body = owner()->getComponent<RigidBody2D>();
    if (body && body->bodyType() == BodyType::Dynamic)
        body->setVelocity(Math::Vec2(0.0f, 0.0f));
}

void NavigationAgent2D::recordVelocity(const Math::Vec2 &before, float deltaTime)
{
    if (deltaTime <= 0.0001f || !owner())
        return;
    mVelocity = (owner()->globalPosition() - before) * (1.0f / deltaTime);
}
} // namespace k2d
