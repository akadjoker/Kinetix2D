#pragma once

#include "k2d/Component.h"

#include <ct/string.hpp>
#include <ct/vector.hpp>
#include <mathc.h>

namespace k2d
{
// Requests a route from Navigation2D. Auto movement is optional: games may
// instead read nextPathPosition() and drive their own physics character.
class NavigationAgent2D final : public Component
{
  public:
    static const ComponentType Type = ComponentType::NavigationAgent;

    NavigationAgent2D();

    bool setTargetPosition(const Math::Vec2& position);
    const Math::Vec2& targetPosition() const
    {
        return mTarget;
    }
    bool hasTarget() const { return mHasTarget; }
    void clearTarget()
    {
        mHasTarget = false;
        clearPath();
    }
    bool repath();
    // The follow target's live position, using the pointer already resolved
    // under the topology gate. Scripts must not cache a node handle instead:
    // handles are never invalidated when an object is destroyed.
    bool followPosition(Math::Vec2& out) const;
    void clearPath();

    void setFollowTargetName(const char* name)
    {
        mFollowTargetName = name ? name : "";
        mFollowTarget = nullptr;
        // Dropping the pointer is not enough: the resolve is gated on the
        // topology version, so a name set after this frame's resolve would
        // stay unresolved for as long as the tree did not change, and the
        // agent would silently never path again.
        mFollowVersion = kUnresolvedFollowVersion;
    }
    const ct::String& followTargetName() const
    {
        return mFollowTargetName;
    }
    bool hasFollowTarget() const
    {
        return !mFollowTargetName.empty();
    }

    void setRepathInterval(float value);
    float repathInterval() const
    {
        return mRepathInterval;
    }
    void setRepathMoveThreshold(float value);
    float repathMoveThreshold() const
    {
        return mRepathMoveThreshold;
    }
    uint32_t repathCount() const
    {
        return mRepathCount;
    }
    const ct::Vector<Math::Vec2>& path() const
    {
        return mPath;
    }
    bool hasPath() const
    {
        return mPathIndex < mPath.size();
    }
    bool isNavigationFinished() const
    {
        return !mHasTarget || mPathIndex >= mPath.size();
    }
    Math::Vec2 nextPathPosition() const;
    void advance();

    void setPathDesiredDistance(float value);
    float pathDesiredDistance() const
    {
        return mPathDesiredDistance;
    }
    void setMaxSpeed(float value);
    float maxSpeed() const
    {
        return mMaxSpeed;
    }
    const Math::Vec2& velocity() const
    {
        return mVelocity;
    }
    void setAutoMove(bool value)
    {
        mAutoMove = value;
    }
    bool autoMove() const
    {
        return mAutoMove;
    }
    void setOrientToPath(bool value)
    {
        mOrientToPath = value;
    }
    bool orientToPath() const
    {
        return mOrientToPath;
    }
    void setRotationLerpSpeed(float value);
    float rotationLerpSpeed() const
    {
        return mRotationLerpSpeed;
    }
    void setRotationOffsetDegrees(float value)
    {
        mRotationOffsetDegrees = value;
    }
    float rotationOffsetDegrees() const
    {
        return mRotationOffsetDegrees;
    }

  protected:
    void onAwake() override;
    void onUpdate(float deltaTime) override;

  private:
    void translateGlobal(const Math::Vec2 &offset);

  protected:

  private:
    bool retarget(const Math::Vec2& position, bool forceRepath);
    void updateFollowTarget(float deltaTime);

    Math::Vec2 mTarget{0.0f};
    Math::Vec2 mLastPathedTarget{0.0f};
    Math::Vec2 mVelocity{0.0f};
    Math::Vec2 mSmoothedAcceleration{0.0f};
    ct::Vector<Math::Vec2> mPath;
    std::size_t mPathIndex = 0;
    float mPathDesiredDistance = 8.0f;
    float mMaxSpeed = 160.0f;
    float mRotationLerpSpeed = 12.0f;
    float mRotationOffsetDegrees = 0.0f;
    float mRepathInterval = 0.25f;
    float mRepathMoveThreshold = 16.0f;
    float mRepathTimer = 0.0f;
    uint32_t mRepathCount = 0;
    ct::String mFollowTargetName;
    GameObject* mFollowTarget = nullptr;
    static const uint32_t kUnresolvedFollowVersion = 0xFFFFFFFFu;

    uint32_t mFollowVersion = kUnresolvedFollowVersion;
    bool mHasTarget = false;
    void recordVelocity(const Math::Vec2 &before, float deltaTime);
    void applySteeringForce(Math::Vec2 force, float deltaTime);

    float mLastDeltaTime = 0.0f;
    bool mHasPathedTarget = false;
    bool mAutoMove = false;
    bool mOrientToPath = false;
};
} // namespace k2d
