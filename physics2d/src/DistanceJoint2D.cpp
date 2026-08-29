#include "k2d/DistanceJoint2D.h"

#include "k2d/RigidBody2D.h"

#include <cmath>

namespace k2d
{

DistanceJoint2D::DistanceJoint2D()
    : mLocalAnchorA(0.0f, 0.0f), mLocalAnchorB(0.0f, 0.0f), mLength(100.0f), mMinLength(100.0f), mMaxLength(100.0f),
      mSpringFrequency(0.0f), mSpringDamping(0.0f), mStiffness(0.0f), mDamping(0.0f), mGamma(0.0f), mBias(0.0f),
      mImpulse(0.0f), mLowerImpulse(0.0f), mUpperImpulse(0.0f), mU(0.0f, 0.0f), mRA(0.0f, 0.0f), mRB(0.0f, 0.0f),
      mCurrentLength(0.0f), mMass(0.0f), mSoftMass(0.0f)
{
}

void DistanceJoint2D::setLength(float length)
{
    mLength = length > kLinearSlop ? length : kLinearSlop;
}

void DistanceJoint2D::setLengthRange(float minLength, float maxLength)
{
    mMinLength = minLength < kLinearSlop ? kLinearSlop : minLength;
    mMaxLength = maxLength < mMinLength ? mMinLength : maxLength;
    mLowerImpulse = 0.0f;
    mUpperImpulse = 0.0f;
}

void DistanceJoint2D::setSpring(float frequencyHz, float dampingRatio)
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

Math::Vec2 DistanceJoint2D::anchorA() const
{
    return mBodyA->GetTransform().Transform(mLocalAnchorA);
}

Math::Vec2 DistanceJoint2D::anchorB() const
{
    return mBodyB->GetTransform().Transform(mLocalAnchorB);
}

void DistanceJoint2D::initVelocity(float dt)
{
    RigidBody2D *a = mBodyA;
    RigidBody2D *b = mBodyB;

    Transform xfA = a->GetTransform();
    Transform xfB = b->GetTransform();

    Math::Vec2 worldAnchorA = xfA.Transform(mLocalAnchorA);
    Math::Vec2 worldAnchorB = xfB.Transform(mLocalAnchorB);
    mRA = worldAnchorA - a->WorldCenter();
    mRB = worldAnchorB - b->WorldCenter();
    mU = worldAnchorB - worldAnchorA;

    mCurrentLength = std::sqrt(Dot(mU, mU));
    if (mCurrentLength > kLinearSlop)
    {
        mU *= 1.0f / mCurrentLength;
    }
    else
    {
        mU = Math::Vec2(0.0f, 0.0f);
        mMass = 0.0f;
        mImpulse = 0.0f;
        mLowerImpulse = 0.0f;
        mUpperImpulse = 0.0f;
    }

    float mA = a->InvMass(), mB = b->InvMass();
    float iA = a->InvI(), iB = b->InvI();

    float crAu = Cross(mRA, mU);
    float crBu = Cross(mRB, mU);
    float invMass = mA + iA * crAu * crAu + mB + iB * crBu * crBu;
    mMass = invMass != 0.0f ? 1.0f / invMass : 0.0f;

    if (mStiffness > 0.0f && mMinLength < mMaxLength)
    {
        float C = mCurrentLength - mLength;

        float h = dt;
        mGamma = h * (mDamping + h * mStiffness);
        mGamma = mGamma != 0.0f ? 1.0f / mGamma : 0.0f;
        mBias = C * h * mStiffness * mGamma;

        invMass += mGamma;
        mSoftMass = invMass != 0.0f ? 1.0f / invMass : 0.0f;
    }
    else
    {
        mGamma = 0.0f;
        mBias = 0.0f;
        mSoftMass = mMass;
    }

    Math::Vec2 P = (mImpulse + mLowerImpulse - mUpperImpulse) * mU;
    a->setVelocity(a->velocity() - mA * P);
    a->SetAngularVelocityRadians(a->AngularVelocityRadians() - iA * Cross(mRA, P));
    b->setVelocity(b->velocity() + mB * P);
    b->SetAngularVelocityRadians(b->AngularVelocityRadians() + iB * Cross(mRB, P));
}

void DistanceJoint2D::solveVelocity(float dt)
{
    RigidBody2D *a = mBodyA;
    RigidBody2D *b = mBodyB;

    float mA = a->InvMass(), mB = b->InvMass();
    float iA = a->InvI(), iB = b->InvI();

    Math::Vec2 vA = a->velocity();
    float wA = a->AngularVelocityRadians();
    Math::Vec2 vB = b->velocity();
    float wB = b->AngularVelocityRadians();

    if (mMinLength < mMaxLength)
    {
        if (mStiffness > 0.0f)
        {
            Math::Vec2 vpA = vA + Cross(wA, mRA);
            Math::Vec2 vpB = vB + Cross(wB, mRB);
            float Cdot = Dot(mU, vpB - vpA);

            float impulse = -mSoftMass * (Cdot + mBias + mGamma * mImpulse);
            mImpulse += impulse;

            Math::Vec2 P = impulse * mU;
            vA -= mA * P;
            wA -= iA * Cross(mRA, P);
            vB += mB * P;
            wB += iB * Cross(mRB, P);
        }

        {
            float C = mCurrentLength - mMinLength;
            float bias = (C > 0.0f ? C : 0.0f) / dt;

            Math::Vec2 vpA = vA + Cross(wA, mRA);
            Math::Vec2 vpB = vB + Cross(wB, mRB);
            float Cdot = Dot(mU, vpB - vpA);

            float impulse = -mMass * (Cdot + bias);
            float oldImpulse = mLowerImpulse;
            mLowerImpulse = mLowerImpulse + impulse > 0.0f ? mLowerImpulse + impulse : 0.0f;
            impulse = mLowerImpulse - oldImpulse;
            Math::Vec2 P = impulse * mU;

            vA -= mA * P;
            wA -= iA * Cross(mRA, P);
            vB += mB * P;
            wB += iB * Cross(mRB, P);
        }

        {
            float C = mMaxLength - mCurrentLength;
            float bias = (C > 0.0f ? C : 0.0f) / dt;

            Math::Vec2 vpA = vA + Cross(wA, mRA);
            Math::Vec2 vpB = vB + Cross(wB, mRB);
            float Cdot = Dot(mU, vpA - vpB);

            float impulse = -mMass * (Cdot + bias);
            float oldImpulse = mUpperImpulse;
            mUpperImpulse = mUpperImpulse + impulse > 0.0f ? mUpperImpulse + impulse : 0.0f;
            impulse = mUpperImpulse - oldImpulse;
            Math::Vec2 P = -impulse * mU;

            vA -= mA * P;
            wA -= iA * Cross(mRA, P);
            vB += mB * P;
            wB += iB * Cross(mRB, P);
        }
    }
    else
    {
        Math::Vec2 vpA = vA + Cross(wA, mRA);
        Math::Vec2 vpB = vB + Cross(wB, mRB);
        float Cdot = Dot(mU, vpB - vpA);

        float impulse = -mMass * Cdot;
        mImpulse += impulse;

        Math::Vec2 P = impulse * mU;
        vA -= mA * P;
        wA -= iA * Cross(mRA, P);
        vB += mB * P;
        wB += iB * Cross(mRB, P);
    }

    a->setVelocity(vA);
    a->SetAngularVelocityRadians(wA);
    b->setVelocity(vB);
    b->SetAngularVelocityRadians(wB);
}

bool DistanceJoint2D::solvePosition()
{
    RigidBody2D *a = mBodyA;
    RigidBody2D *b = mBodyB;

    Transform xfA = a->GetTransform();
    Transform xfB = b->GetTransform();
    Math::Vec2 worldAnchorA = xfA.Transform(mLocalAnchorA);
    Math::Vec2 worldAnchorB = xfB.Transform(mLocalAnchorB);
    Math::Vec2 rA = worldAnchorA - a->WorldCenter();
    Math::Vec2 rB = worldAnchorB - b->WorldCenter();
    Math::Vec2 u = worldAnchorB - worldAnchorA;

    float length = std::sqrt(Dot(u, u));
    if (length > kEpsilon)
        u *= 1.0f / length;

    float C;
    if (mMinLength == mMaxLength)
        C = length - mMinLength;
    else if (length < mMinLength)
        C = length - mMinLength;
    else if (mMaxLength < length)
        C = length - mMaxLength;
    else
        return true;

    float impulse = -mMass * C;
    Math::Vec2 P = impulse * u;

    a->ShiftCenter(-a->InvMass() * P, -a->InvI() * Cross(rA, P));
    b->ShiftCenter(b->InvMass() * P, b->InvI() * Cross(rB, P));

    return std::fabs(C) < kLinearSlop;
}

}
