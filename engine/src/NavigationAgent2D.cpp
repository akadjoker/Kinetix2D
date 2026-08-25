#include "k2d/NavigationAgent2D.h"

#include "k2d/GameObject.h"
#include "k2d/Navigation2D.h"
#include "k2d/Scene.h"

#include <algorithm>
#include <cmath>

namespace k2d
{
NavigationAgent2D::NavigationAgent2D() : Component(Type, ComponentEventUpdate)
{
}

bool NavigationAgent2D::setTargetPosition(const Math::Vec2& position)
{
    mTarget = position;
    mHasTarget = true;
    return repath();
}

bool NavigationAgent2D::repath()
{
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
    mPathDesiredDistance = std::max(0.01f, value);
}

void NavigationAgent2D::setMaxSpeed(float value)
{
    mMaxSpeed = std::max(0.0f, value);
}

void NavigationAgent2D::setRotationLerpSpeed(float value)
{
    mRotationLerpSpeed = std::max(0.0f, value);
}

void NavigationAgent2D::onAwake()
{
    if (mHasTarget)
        repath();
}

void NavigationAgent2D::onUpdate(float deltaTime)
{
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
    const float distance = std::min(length, mMaxSpeed * deltaTime);
    owner()->translate(delta * (distance / length));
    advance();
}
} // namespace k2d
