#include "kx/joints/distancejoint.h"

#include "kx/body.h"

#include <cmath>

namespace kx
{

    DistanceJoint::DistanceJoint(Body *a, Body *b, const glm::vec2 &worldAnchorA, const glm::vec2 &worldAnchorB)
        : Joint(JointType::Distance, a, b),
          mStiffness(0.0f), mDamping(0.0f),
          mGamma(0.0f), mBias(0.0f), mImpulse(0.0f), mLowerImpulse(0.0f), mUpperImpulse(0.0f),
          mU(0.0f, 0.0f), mRA(0.0f, 0.0f), mRB(0.0f, 0.0f),
          mCurrentLength(0.0f), mMass(0.0f), mSoftMass(0.0f)
    {
        mLocalAnchorA = InvTransformPoint(a->GetTransform(), worldAnchorA);
        mLocalAnchorB = InvTransformPoint(b->GetTransform(), worldAnchorB);

        glm::vec2 d = worldAnchorB - worldAnchorA;
        mLength = std::sqrt(Dot(d, d));
        if (mLength < kLinearSlop)
            mLength = kLinearSlop;
        mMinLength = mLength;
        mMaxLength = mLength;
    }

    void DistanceJoint::SetSpring(float frequencyHz, float dampingRatio)
    {
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

    void DistanceJoint::SetLengthRange(float minLength, float maxLength)
    {
        mMinLength = minLength < kLinearSlop ? kLinearSlop : minLength;
        mMaxLength = maxLength < mMinLength ? mMinLength : maxLength;
        mLowerImpulse = 0.0f;
        mUpperImpulse = 0.0f;
    }

    glm::vec2 DistanceJoint::AnchorA() const
    {
        return mBodyA->GetTransform().Transform(mLocalAnchorA);
    }

    glm::vec2 DistanceJoint::AnchorB() const
    {
        return mBodyB->GetTransform().Transform(mLocalAnchorB);
    }

    void DistanceJoint::InitVelocity(float dt)
    {
        Body *a = mBodyA;
        Body *b = mBodyB;

        Transform xfA = a->GetTransform();
        Transform xfB = b->GetTransform();

        glm::vec2 worldAnchorA = xfA.Transform(mLocalAnchorA);
        glm::vec2 worldAnchorB = xfB.Transform(mLocalAnchorB);
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
            mU = glm::vec2(0.0f, 0.0f);
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

        glm::vec2 P = (mImpulse + mLowerImpulse - mUpperImpulse) * mU;
        a->SetVelocity(a->Velocity() - mA * P);
        a->SetAngularVelocity(a->AngularVelocity() - iA * Cross(mRA, P));
        b->SetVelocity(b->Velocity() + mB * P);
        b->SetAngularVelocity(b->AngularVelocity() + iB * Cross(mRB, P));
    }

    void DistanceJoint::SolveVelocity(float dt)
    {
        Body *a = mBodyA;
        Body *b = mBodyB;

        float mA = a->InvMass(), mB = b->InvMass();
        float iA = a->InvI(), iB = b->InvI();

        glm::vec2 vA = a->Velocity();
        float wA = a->AngularVelocity();
        glm::vec2 vB = b->Velocity();
        float wB = b->AngularVelocity();

        if (mMinLength < mMaxLength)
        {
            if (mStiffness > 0.0f)
            {
                glm::vec2 vpA = vA + Cross(wA, mRA);
                glm::vec2 vpB = vB + Cross(wB, mRB);
                float Cdot = Dot(mU, vpB - vpA);

                float impulse = -mSoftMass * (Cdot + mBias + mGamma * mImpulse);
                mImpulse += impulse;

                glm::vec2 P = impulse * mU;
                vA -= mA * P;
                wA -= iA * Cross(mRA, P);
                vB += mB * P;
                wB += iB * Cross(mRB, P);
            }

            {
                float C = mCurrentLength - mMinLength;
                float bias = (C > 0.0f ? C : 0.0f) / dt;

                glm::vec2 vpA = vA + Cross(wA, mRA);
                glm::vec2 vpB = vB + Cross(wB, mRB);
                float Cdot = Dot(mU, vpB - vpA);

                float impulse = -mMass * (Cdot + bias);
                float oldImpulse = mLowerImpulse;
                mLowerImpulse = mLowerImpulse + impulse > 0.0f ? mLowerImpulse + impulse : 0.0f;
                impulse = mLowerImpulse - oldImpulse;
                glm::vec2 P = impulse * mU;

                vA -= mA * P;
                wA -= iA * Cross(mRA, P);
                vB += mB * P;
                wB += iB * Cross(mRB, P);
            }

            {
                float C = mMaxLength - mCurrentLength;
                float bias = (C > 0.0f ? C : 0.0f) / dt;

                glm::vec2 vpA = vA + Cross(wA, mRA);
                glm::vec2 vpB = vB + Cross(wB, mRB);
                float Cdot = Dot(mU, vpA - vpB);

                float impulse = -mMass * (Cdot + bias);
                float oldImpulse = mUpperImpulse;
                mUpperImpulse = mUpperImpulse + impulse > 0.0f ? mUpperImpulse + impulse : 0.0f;
                impulse = mUpperImpulse - oldImpulse;
                glm::vec2 P = -impulse * mU;

                vA -= mA * P;
                wA -= iA * Cross(mRA, P);
                vB += mB * P;
                wB += iB * Cross(mRB, P);
            }
        }
        else
        {
            glm::vec2 vpA = vA + Cross(wA, mRA);
            glm::vec2 vpB = vB + Cross(wB, mRB);
            float Cdot = Dot(mU, vpB - vpA);

            float impulse = -mMass * Cdot;
            mImpulse += impulse;

            glm::vec2 P = impulse * mU;
            vA -= mA * P;
            wA -= iA * Cross(mRA, P);
            vB += mB * P;
            wB += iB * Cross(mRB, P);
        }

        a->SetVelocity(vA);
        a->SetAngularVelocity(wA);
        b->SetVelocity(vB);
        b->SetAngularVelocity(wB);
    }

    bool DistanceJoint::SolvePosition()
    {
        Body *a = mBodyA;
        Body *b = mBodyB;

        Transform xfA = a->GetTransform();
        Transform xfB = b->GetTransform();
        glm::vec2 worldAnchorA = xfA.Transform(mLocalAnchorA);
        glm::vec2 worldAnchorB = xfB.Transform(mLocalAnchorB);
        glm::vec2 rA = worldAnchorA - a->WorldCenter();
        glm::vec2 rB = worldAnchorB - b->WorldCenter();
        glm::vec2 u = worldAnchorB - worldAnchorA;

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
        glm::vec2 P = impulse * u;

        a->ShiftCenter(-a->InvMass() * P, -a->InvI() * Cross(rA, P));
        b->ShiftCenter(b->InvMass() * P, b->InvI() * Cross(rB, P));

        return std::fabs(C) < kLinearSlop;
    }

} 