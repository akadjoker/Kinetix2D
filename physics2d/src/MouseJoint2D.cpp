#include "k2d/MouseJoint2D.h"

#include "k2d/GameObject.h"
#include "k2d/RigidBody2D.h"
#include "k2d/Scene.h"

#include <cmath>

namespace k2d
{

MouseJoint2D::MouseJoint2D()
    : mTarget(0.0f, 0.0f), mLocalAnchor(0.0f, 0.0f), mMaxForce(1000.0f), mSpringFrequency(5.0f),
      mSpringDamping(0.7f), mStiffness(0.0f), mDamping(0.0f), mRB(0.0f, 0.0f), mC(0.0f, 0.0f),
      mImpulse(0.0f, 0.0f), mGamma(0.0f), mMass00(0.0f), mMass01(0.0f), mMass10(0.0f), mMass11(0.0f)
{
}

void MouseJoint2D::resolve()
{
    GameObject *object = owner();
    Scene *scene = object ? object->scene() : nullptr;
    if (!scene || mResolvedVersion == scene->topologyVersion())
        return;

    mBodyA = nullptr;
    const bool wasConnected = mBodyB != nullptr;
    mBodyB = object->getComponent<RigidBody2D>();
    if (!mBodyB)
        return;

    mResolvedVersion = scene->topologyVersion();
    if (!wasConnected)
    {
        mLocalAnchor = InvTransformPoint(mBodyB->GetTransform(), mTarget);
        setSpring(mSpringFrequency, mSpringDamping);
    }
}

void MouseJoint2D::setSpring(float frequencyHz, float dampingRatio)
{
    mSpringFrequency = frequencyHz;
    mSpringDamping = dampingRatio;
    if (!mBodyB)
        return;

    float mass = mBodyB->Mass();
    float omega = 2.0f * kPi * frequencyHz;
    mStiffness = mass * omega * omega;
    mDamping = 2.0f * mass * dampingRatio * omega;
}

Math::Vec2 MouseJoint2D::anchorB() const
{
    return mBodyB->GetTransform().Transform(mLocalAnchor);
}

void MouseJoint2D::initVelocity(float dt)
{
    RigidBody2D *b = mBodyB;
    float invMass = b->InvMass();
    float invI = b->InvI();

    float h = dt;
    mGamma = h * (mDamping + h * mStiffness);
    if (mGamma != 0.0f)
        mGamma = 1.0f / mGamma;
    float beta = h * mStiffness * mGamma;

    Transform xf = b->GetTransform();
    mRB = xf.Transform(mLocalAnchor) - b->WorldCenter();

    float k00 = invMass + invI * mRB.y * mRB.y + mGamma;
    float k01 = -invI * mRB.x * mRB.y;
    float k11 = invMass + invI * mRB.x * mRB.x + mGamma;
    float det = k00 * k11 - k01 * k01;
    if (det != 0.0f)
        det = 1.0f / det;
    mMass00 = det * k11;
    mMass01 = -det * k01;
    mMass10 = -det * k01;
    mMass11 = det * k00;

    mC = (b->WorldCenter() + mRB - mTarget) * beta;

    float damp = 1.0f - 0.02f * (60.0f * dt);
    if (damp < 0.0f)
        damp = 0.0f;
    b->SetAngularVelocityRadians(b->AngularVelocityRadians() * damp);

    b->setVelocity(b->velocity() + invMass * mImpulse);
    b->SetAngularVelocityRadians(b->AngularVelocityRadians() + invI * Cross(mRB, mImpulse));
}

void MouseJoint2D::solveVelocity(float dt)
{
    RigidBody2D *b = mBodyB;

    Math::Vec2 cdot = b->velocity() + Cross(b->AngularVelocityRadians(), mRB);
    Math::Vec2 rhs = -(cdot + mC + mGamma * mImpulse);
    Math::Vec2 impulse(mMass00 * rhs.x + mMass01 * rhs.y, mMass10 * rhs.x + mMass11 * rhs.y);

    Math::Vec2 oldImpulse = mImpulse;
    mImpulse += impulse;
    float maxImpulse = dt * mMaxForce;
    float lenSq = mImpulse.x * mImpulse.x + mImpulse.y * mImpulse.y;
    if (lenSq > maxImpulse * maxImpulse)
        mImpulse *= maxImpulse / sqrtf(lenSq);
    impulse = mImpulse - oldImpulse;

    b->setVelocity(b->velocity() + b->InvMass() * impulse);
    b->SetAngularVelocityRadians(b->AngularVelocityRadians() + b->InvI() * Cross(mRB, impulse));
}

}
