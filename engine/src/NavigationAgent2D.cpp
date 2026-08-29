#include "k2d/NavigationAgent2D.h"
#include "k2d/Utils.h"

#include "k2d/GameObject.h"
#include "k2d/Geometry2D.h"
#include "k2d/Navigation2D.h"
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
    if (mHasTarget)
        retarget(mTarget, true);
}

void NavigationAgent2D::updateFollowTarget(float deltaTime)
{
    if (!hasFollowTarget() || !owner() || !owner()->scene())
        return;
    Scene* scene = owner()->scene();
    if (!mFollowTarget || mFollowVersion != scene->topologyVersion())
    {
        mFollowTarget = scene->find(mFollowTargetName.c_str());
        mFollowVersion = scene->topologyVersion();
    }
    if (!mFollowTarget)
        return;
    const Math::Vec2 candidate = mFollowTarget->globalPosition();
    if (!hasPath())
    {
        retarget(candidate, false);
        mRepathTimer = 0.0f;
        return;
    }
    mRepathTimer += deltaTime;
    if (mRepathTimer >= mRepathInterval)
    {
        retarget(candidate, false);
        mRepathTimer = 0.0f;
        return;
    }
    mTarget = candidate;
    mHasTarget = true;
}

void NavigationAgent2D::onUpdate(float deltaTime)
{
    updateFollowTarget(deltaTime);
    advance();
    if (!hasPath() || !owner())
        return;
    const Math::Vec2 delta = nextPathPosition() - owner()->globalPosition();
    const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (length <= 0.0001f)
        return;
    if (mOrientToPath)
    {
        constexpr float kRadiansToDegrees = 57.2957795131f;
        const float wanted = std::atan2(delta.y, delta.x) * kRadiansToDegrees + mRotationOffsetDegrees;
        const float current = owner()->rotationDegrees();
        float angleDelta = std::fmod(wanted - current + 180.0f, 360.0f);
        if (angleDelta < 0.0f)
            angleDelta += 360.0f;
        angleDelta -= 180.0f;
        const float alpha = mRotationLerpSpeed <= 0.0f ? 1.0f : 1.0f - std::exp(-mRotationLerpSpeed * deltaTime);
        owner()->setRotationDegrees(current + angleDelta * alpha);
    }
    if (!mAutoMove)
        return;
    const float distance = Min(length, mMaxSpeed * deltaTime);
    owner()->translate(delta * (distance / length));
    advance();
}
} // namespace k2d
