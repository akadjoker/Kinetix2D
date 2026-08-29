#include "k2d/CharacterBody2D.h"

#include "k2d/GameObject.h"
#include "k2d/RigidBody2D.h"
#include "k2d/Scene.h"

#include <cmath>

namespace k2d
{
namespace
{
constexpr float kRadToDeg = 57.29577951308232088f;

Math::Vec2 globalPositionFromLocal(const GameObject& object, float x, float y)
{
    const GameObject* parent = object.parent();
    return parent ? parent->globalPosition() + Math::Vec2(x, y) : Math::Vec2(x, y);
}
} // namespace

CharacterBody2D::CharacterBody2D()
    : Component(Type, ComponentEventNone), mVelocity(0.0f, 0.0f), mUpDirection(0.0f, -1.0f), mFloorNormal(0.0f, 0.0f),
      mSlideCollisions(), mSafeMargin(kLinearSlop), mFloorMaxAngleDegrees(45.0f), mMaxSlides(4),
      mMotionMode(MotionMode::Floating), mOnFloor(false), mOnWall(false), mOnCeiling(false)
{
}

void CharacterBody2D::onAwake()
{
    if (RigidBody2D* body = rigidBody())
        body->setBodyType(BodyType::Kinematic);
}

RigidBody2D* CharacterBody2D::rigidBody() const
{
    return owner() ? owner()->getComponent<RigidBody2D>() : nullptr;
}

void CharacterBody2D::applyWorldPosition(const Math::Vec2& position) const
{
    GameObject* object = owner();
    if (!object)
        return;
    if (GameObject* parent = object->parent())
        object->setPosition(position - parent->globalPosition());
    else
        object->setPosition(position);
}

CollisionInfo CharacterBody2D::moveAndCollide(const Math::Vec2& motion, bool testOnly)
{
    CollisionInfo collision;
    collision.self = owner();
    RigidBody2D* rigid = rigidBody();
    Scene* scene = owner() ? owner()->scene() : nullptr;
    if (!rigid || !rigid->inWorld() || !scene || rigid->bodyType() != BodyType::Kinematic)
        return collision;

    MotionResult result;
    scene->testMotion(*rigid, motion, result, mSafeMargin);
    collision.hit = result.hit;
    collision.other = Scene::objectForBody(result.body);
    collision.point = result.point;
    collision.normal = result.normal;
    collision.travel = result.travel;
    collision.remainder = result.remainder;
    collision.fraction = result.fraction;

    // testMotion reports the whole motion as travel when nothing was hit, so
    // the move has to be applied either way - returning early here meant a
    // character could only ever move by colliding with something.
    if (!testOnly)
    {
        const Math::Vec2 target = rigid->Position() + result.travel;
        rigid->SetPosition(target);
        applyWorldPosition(target);
    }
    return collision;
}

bool CharacterBody2D::testMove(const Math::Vec2& motion, CollisionInfo* out) const
{
    CharacterBody2D* self = const_cast<CharacterBody2D*>(this);
    CollisionInfo collision = self->moveAndCollide(motion, true);
    if (out)
        *out = collision;
    return collision.hit;
}

bool CharacterBody2D::placeFree(float x, float y) const
{
    RigidBody2D* rigid = rigidBody();
    Scene* scene = owner() ? owner()->scene() : nullptr;
    if (!rigid || !rigid->inWorld() || !scene)
        return true;
    MotionResult result;
    return !scene->testPosition(*rigid, globalPositionFromLocal(*owner(), x, y), result);
}

GameObject* CharacterBody2D::placeMeeting(float x, float y) const
{
    RigidBody2D* rigid = rigidBody();
    Scene* scene = owner() ? owner()->scene() : nullptr;
    if (!rigid || !rigid->inWorld() || !scene)
        return nullptr;
    MotionResult result;
    scene->testPosition(*rigid, globalPositionFromLocal(*owner(), x, y), result);
    return Scene::objectForBody(result.body);
}

void CharacterBody2D::classifyCollision(const CollisionInfo& collision)
{
    if (mMotionMode == MotionMode::Floating)
    {
        mOnWall = true;
        return;
    }
    const float threshold = std::cos(mFloorMaxAngleDegrees / kRadToDeg);
    const float upDot = collision.normal.Dot(mUpDirection);
    if (upDot >= threshold)
    {
        mOnFloor = true;
        mFloorNormal = collision.normal;
    }
    else if (upDot <= -threshold)
        mOnCeiling = true;
    else
        mOnWall = true;
}

bool CharacterBody2D::moveAndSlide()
{
    Scene* scene = owner() ? owner()->scene() : nullptr;
    const float delta = scene ? scene->fixedTimeStep() : 0.0f;
    mOnFloor = mOnWall = mOnCeiling = false;
    mFloorNormal = Math::Vec2(0.0f, 0.0f);
    mSlideCollisions.clear();
    if (delta <= 0.0f)
        return false;

    Math::Vec2 motion = mVelocity * delta;
    for (int index = 0; index < mMaxSlides && motion.LengthSquared() > 0.0000001f; ++index)
    {
        CollisionInfo collision = moveAndCollide(motion);
        if (!collision.hit)
            break;
        mSlideCollisions.push_back(collision);
        classifyCollision(collision);

        const float remainingIntoSurface = collision.remainder.Dot(collision.normal);
        motion = collision.remainder - collision.normal * remainingIntoSurface;
        const float velocityIntoSurface = mVelocity.Dot(collision.normal);
        if (velocityIntoSurface < 0.0f)
            mVelocity -= collision.normal * velocityIntoSurface;
        if (motion.Dot(mVelocity) <= 0.0f)
            break;
    }
    return !mSlideCollisions.empty();
}
} // namespace k2d
