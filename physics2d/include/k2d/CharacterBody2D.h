#pragma once

#include "k2d/Component.h"
#include "k2d/CollisionInfo.h"

#include <ct/vector.hpp>

namespace k2d
{

class RigidBody2D;

// Script-driven kinematic body. It uses the RigidBody2D + Collider2D on
// the same GameObject, but never runs through the impulse solver itself.
class CharacterBody2D : public Component
{
  public:
    static const ComponentType Type = ComponentType::CharacterBody;

    enum class MotionMode : unsigned char
    {
        Floating,
        Grounded
    };

    CharacterBody2D();

    const Math::Vec2& velocity() const
    {
        return mVelocity;
    }
    void setVelocity(const Math::Vec2& velocity)
    {
        mVelocity = velocity;
    }

    float safeMargin() const
    {
        return mSafeMargin;
    }
    void setSafeMargin(float margin)
    {
        mSafeMargin = margin >= 0.0f ? margin : 0.0f;
    }
    int maxSlides() const
    {
        return mMaxSlides;
    }
    void setMaxSlides(int count)
    {
        mMaxSlides = count > 0 ? count : 1;
    }

    MotionMode motionMode() const
    {
        return mMotionMode;
    }
    void setMotionMode(MotionMode mode)
    {
        mMotionMode = mode;
    }
    const Math::Vec2& upDirection() const
    {
        return mUpDirection;
    }
    void setUpDirection(const Math::Vec2& up)
    {
        mUpDirection = up.LengthSquared() > 0.0f ? up.Normalized() : Math::Vec2(0.0f, -1.0f);
    }
    float floorMaxAngleDegrees() const
    {
        return mFloorMaxAngleDegrees;
    }
    void setFloorMaxAngleDegrees(float angle)
    {
        mFloorMaxAngleDegrees = angle >= 0.0f ? angle : 0.0f;
    }

    CollisionInfo moveAndCollide(const Math::Vec2& motion, bool testOnly = false);
    bool moveAndSlide();
    bool testMove(const Math::Vec2& motion, CollisionInfo* out = nullptr) const;
    bool placeFree(float x, float y) const;
    GameObject* placeMeeting(float x, float y) const;

    bool isOnFloor() const
    {
        return mOnFloor;
    }
    bool isOnWall() const
    {
        return mOnWall;
    }
    bool isOnCeiling() const
    {
        return mOnCeiling;
    }
    const Math::Vec2& floorNormal() const
    {
        return mFloorNormal;
    }
    std::size_t slideCollisionCount() const
    {
        return mSlideCollisions.size();
    }
    const CollisionInfo* slideCollision(std::size_t index) const
    {
        return index < mSlideCollisions.size() ? &mSlideCollisions[index] : nullptr;
    }

  protected:
    void onAwake() override;

  private:
    RigidBody2D* rigidBody() const;
    void applyWorldPosition(const Math::Vec2& position) const;
    void classifyCollision(const CollisionInfo& collision);

    Math::Vec2 mVelocity;
    Math::Vec2 mUpDirection;
    Math::Vec2 mFloorNormal;
    ct::Vector<CollisionInfo> mSlideCollisions;
    float mSafeMargin;
    float mFloorMaxAngleDegrees;
    int mMaxSlides;
    MotionMode mMotionMode;
    bool mOnFloor;
    bool mOnWall;
    bool mOnCeiling;
};

} // namespace k2d
