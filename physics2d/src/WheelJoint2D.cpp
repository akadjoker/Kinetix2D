#include "k2d/WheelJoint2D.h"

#include "k2d/RigidBody2D.h"

#include <cmath>

namespace k2d
{

WheelJoint2D::WheelJoint2D()
    : mLocalAnchorA(0.0f, 0.0f), mLocalAnchorB(0.0f, 0.0f), mLocalXAxisA(0.0f, 1.0f),
      mLocalYAxisA(Cross(1.0f, mLocalXAxisA)), mImpulse(0.0f), mMotorImpulse(0.0f), mSpringImpulse(0.0f),
      mMaxMotorTorque(0.0f), mMotorSpeed(0.0f), mEnableMotor(false), mSpringFrequency(4.0f), mSpringDamping(0.7f),
      mStiffness(0.0f), mDamping(0.0f), mAx(0.0f, 0.0f), mAy(0.0f, 0.0f), mSAx(0.0f), mSBx(0.0f), mSAy(0.0f),
      mSBy(0.0f), mMass(0.0f), mMotorMass(0.0f), mAxialMass(0.0f), mSpringMass(0.0f), mBias(0.0f), mGamma(0.0f)
{
}

void WheelJoint2D::setLocalAxisA(const Math::Vec2 &axis)
{
    mLocalXAxisA = axis;
    mLocalYAxisA = Cross(1.0f, mLocalXAxisA);
}

void WheelJoint2D::setMotor(bool enabled, float speed, float maxTorque)
{
    mEnableMotor = enabled;
    mMotorSpeed = speed;
    mMaxMotorTorque = maxTorque;
}

void WheelJoint2D::setSpring(float frequencyHz, float dampingRatio)
{
    mSpringFrequency = frequencyHz;
    mSpringDamping = dampingRatio;
    if (!mBodyA || !mBodyB)
        return;

    float massA = mBodyA->Mass();
    float massB = mBodyB->Mass();
    float mass;
    if (massA > 0.0f && massB > 0.0f)
        mass = massA * massB / (massA + massB);
    else if (massA > 0.0f)
        mass = massA;
    else
        mass = massB;

    float omega = 2.0f * kPi * frequencyHz;
    mStiffness = mass * omega * omega;
    mDamping = 2.0f * mass * dampingRatio * omega;
}

Math::Vec2 WheelJoint2D::anchorA() const
{
    return mBodyA->GetTransform().Transform(mLocalAnchorA);
}

Math::Vec2 WheelJoint2D::anchorB() const
{
    return mBodyB->GetTransform().Transform(mLocalAnchorB);
}

void WheelJoint2D::initVelocity(float dt)
{
    RigidBody2D *a = mBodyA;
    RigidBody2D *b = mBodyB;

    Transform xfA = a->GetTransform();
    Transform xfB = b->GetTransform();

    Math::Vec2 worldAnchorA = xfA.Transform(mLocalAnchorA);
    Math::Vec2 worldAnchorB = xfB.Transform(mLocalAnchorB);
    Math::Vec2 rA = worldAnchorA - a->WorldCenter();
    Math::Vec2 rB = worldAnchorB - b->WorldCenter();
    Math::Vec2 d = worldAnchorB - worldAnchorA;

    float mA = a->InvMass(), mB = b->InvMass();
    float iA = a->InvI(), iB = b->InvI();

    mAy = Rotate(xfA, mLocalYAxisA);
    mSAy = Cross(d + rA, mAy);
    mSBy = Cross(rB, mAy);

    mMass = mA + mB + iA * mSAy * mSAy + iB * mSBy * mSBy;
    if (mMass > 0.0f)
        mMass = 1.0f / mMass;

    mAx = Rotate(xfA, mLocalXAxisA);
    mSAx = Cross(d + rA, mAx);
    mSBx = Cross(rB, mAx);

    float invMass = mA + mB + iA * mSAx * mSAx + iB * mSBx * mSBx;
    mAxialMass = invMass > 0.0f ? 1.0f / invMass : 0.0f;

    mSpringMass = 0.0f;
    mBias = 0.0f;
    mGamma = 0.0f;

    if (mStiffness > 0.0f && invMass > 0.0f)
    {
        float C = Dot(d, mAx);

        float h = dt;
        mGamma = h * (mDamping + h * mStiffness);
        if (mGamma > 0.0f)
            mGamma = 1.0f / mGamma;

        mBias = C * h * mStiffness * mGamma;

        mSpringMass = invMass + mGamma;
        if (mSpringMass > 0.0f)
            mSpringMass = 1.0f / mSpringMass;
    }
    else
    {
        mSpringImpulse = 0.0f;
    }

    if (mEnableMotor)
    {
        mMotorMass = iA + iB;
        if (mMotorMass > 0.0f)
            mMotorMass = 1.0f / mMotorMass;
    }
    else
    {
        mMotorMass = 0.0f;
        mMotorImpulse = 0.0f;
    }

    Math::Vec2 P = mImpulse * mAy + mSpringImpulse * mAx;
    float LA = mImpulse * mSAy + mSpringImpulse * mSAx + mMotorImpulse;
    float LB = mImpulse * mSBy + mSpringImpulse * mSBx + mMotorImpulse;

    a->setVelocity(a->velocity() - mA * P);
    a->SetAngularVelocityRadians(a->AngularVelocityRadians() - iA * LA);
    b->setVelocity(b->velocity() + mB * P);
    b->SetAngularVelocityRadians(b->AngularVelocityRadians() + iB * LB);
}

void WheelJoint2D::solveVelocity(float dt)
{
    RigidBody2D *a = mBodyA;
    RigidBody2D *b = mBodyB;

    float mA = a->InvMass(), mB = b->InvMass();
    float iA = a->InvI(), iB = b->InvI();

    Math::Vec2 vA = a->velocity();
    float wA = a->AngularVelocityRadians();
    Math::Vec2 vB = b->velocity();
    float wB = b->AngularVelocityRadians();

    {
        float Cdot = Dot(mAx, vB - vA) + mSBx * wB - mSAx * wA;
        float impulse = -mSpringMass * (Cdot + mBias + mGamma * mSpringImpulse);
        mSpringImpulse += impulse;

        Math::Vec2 P = impulse * mAx;
        float LA = impulse * mSAx;
        float LB = impulse * mSBx;

        vA -= mA * P;
        wA -= iA * LA;
        vB += mB * P;
        wB += iB * LB;
    }

    {
        float Cdot = wB - wA - mMotorSpeed;
        float impulse = -mMotorMass * Cdot;

        float oldImpulse = mMotorImpulse;
        float maxImpulse = dt * mMaxMotorTorque;
        float newImpulse = mMotorImpulse + impulse;
        if (newImpulse < -maxImpulse)
            newImpulse = -maxImpulse;
        else if (newImpulse > maxImpulse)
            newImpulse = maxImpulse;
        mMotorImpulse = newImpulse;
        impulse = mMotorImpulse - oldImpulse;

        wA -= iA * impulse;
        wB += iB * impulse;
    }

    {
        float Cdot = Dot(mAy, vB - vA) + mSBy * wB - mSAy * wA;
        float impulse = -mMass * Cdot;
        mImpulse += impulse;

        Math::Vec2 P = impulse * mAy;
        float LA = impulse * mSAy;
        float LB = impulse * mSBy;

        vA -= mA * P;
        wA -= iA * LA;
        vB += mB * P;
        wB += iB * LB;
    }

    a->setVelocity(vA);
    a->SetAngularVelocityRadians(wA);
    b->setVelocity(vB);
    b->SetAngularVelocityRadians(wB);
}

bool WheelJoint2D::solvePosition()
{
    RigidBody2D *a = mBodyA;
    RigidBody2D *b = mBodyB;

    Transform xfA = a->GetTransform();
    Transform xfB = b->GetTransform();

    Math::Vec2 worldAnchorA = xfA.Transform(mLocalAnchorA);
    Math::Vec2 worldAnchorB = xfB.Transform(mLocalAnchorB);
    Math::Vec2 rA = worldAnchorA - a->WorldCenter();
    Math::Vec2 rB = worldAnchorB - b->WorldCenter();
    Math::Vec2 d = worldAnchorB - worldAnchorA;

    Math::Vec2 ay = Rotate(xfA, mLocalYAxisA);

    float sAy = Cross(d + rA, ay);
    float sBy = Cross(rB, ay);

    float C = Dot(d, ay);

    float invMass = a->InvMass() + b->InvMass() + a->InvI() * mSAy * mSAy + b->InvI() * mSBy * mSBy;

    float impulse = invMass != 0.0f ? -C / invMass : 0.0f;

    Math::Vec2 P = impulse * ay;
    float LA = impulse * sAy;
    float LB = impulse * sBy;

    a->ShiftCenter(-a->InvMass() * P, -a->InvI() * LA);
    b->ShiftCenter(b->InvMass() * P, b->InvI() * LB);

    return std::fabs(C) <= kLinearSlop;
}

}
