#include "kx/joints/wheeljoint.h"

#include "kx/body.h"

#include <cmath>

namespace kx
{

    WheelJoint::WheelJoint(Body *chassis, Body *wheel, const glm::vec2 &worldAnchor, const glm::vec2 &worldAxis,
                           float frequencyHz, float dampingRatio)
        : Joint(JointType::Wheel, chassis, wheel),
          mImpulse(0.0f), mMotorImpulse(0.0f), mSpringImpulse(0.0f),
          mMaxMotorTorque(0.0f), mMotorSpeed(0.0f), mEnableMotor(false),
          mAx(0.0f, 0.0f), mAy(0.0f, 0.0f),
          mSAx(0.0f), mSBx(0.0f), mSAy(0.0f), mSBy(0.0f),
          mMass(0.0f), mMotorMass(0.0f), mAxialMass(0.0f), mSpringMass(0.0f),
          mBias(0.0f), mGamma(0.0f)
    {
        Transform xfA = chassis->GetTransform();
        Transform xfB = wheel->GetTransform();
        mLocalAnchorA = InvTransformPoint(xfA, worldAnchor);
        mLocalAnchorB = InvTransformPoint(xfB, worldAnchor);
        mLocalXAxisA = InvRotate(xfA, worldAxis);
        mLocalYAxisA = Cross(1.0f, mLocalXAxisA);

        SetSpring(frequencyHz, dampingRatio);
    }

    void WheelJoint::SetMotor(bool enabled, float speed, float maxTorque)
    {
        mEnableMotor = enabled;
        mMotorSpeed = speed;
        mMaxMotorTorque = maxTorque;
    }

    void WheelJoint::SetSpring(float frequencyHz, float dampingRatio)
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

    glm::vec2 WheelJoint::AnchorA() const
    {
        return mBodyA->GetTransform().Transform(mLocalAnchorA);
    }

    glm::vec2 WheelJoint::AnchorB() const
    {
        return mBodyB->GetTransform().Transform(mLocalAnchorB);
    }

    void WheelJoint::InitVelocity(float dt)
    {
        Body *a = mBodyA;
        Body *b = mBodyB;

        Transform xfA = a->GetTransform();
        Transform xfB = b->GetTransform();

        glm::vec2 worldAnchorA = xfA.Transform(mLocalAnchorA);
        glm::vec2 worldAnchorB = xfB.Transform(mLocalAnchorB);
        glm::vec2 rA = worldAnchorA - a->WorldCenter();
        glm::vec2 rB = worldAnchorB - b->WorldCenter();
        glm::vec2 d = worldAnchorB - worldAnchorA;

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

        glm::vec2 P = mImpulse * mAy + mSpringImpulse * mAx;
        float LA = mImpulse * mSAy + mSpringImpulse * mSAx + mMotorImpulse;
        float LB = mImpulse * mSBy + mSpringImpulse * mSBx + mMotorImpulse;

        a->SetVelocity(a->Velocity() - mA * P);
        a->SetAngularVelocity(a->AngularVelocity() - iA * LA);
        b->SetVelocity(b->Velocity() + mB * P);
        b->SetAngularVelocity(b->AngularVelocity() + iB * LB);
    }

    void WheelJoint::SolveVelocity(float dt)
    {
        Body *a = mBodyA;
        Body *b = mBodyB;

        float mA = a->InvMass(), mB = b->InvMass();
        float iA = a->InvI(), iB = b->InvI();

        glm::vec2 vA = a->Velocity();
        float wA = a->AngularVelocity();
        glm::vec2 vB = b->Velocity();
        float wB = b->AngularVelocity();

        {
            float Cdot = Dot(mAx, vB - vA) + mSBx * wB - mSAx * wA;
            float impulse = -mSpringMass * (Cdot + mBias + mGamma * mSpringImpulse);
            mSpringImpulse += impulse;

            glm::vec2 P = impulse * mAx;
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

            glm::vec2 P = impulse * mAy;
            float LA = impulse * mSAy;
            float LB = impulse * mSBy;

            vA -= mA * P;
            wA -= iA * LA;
            vB += mB * P;
            wB += iB * LB;
        }

        a->SetVelocity(vA);
        a->SetAngularVelocity(wA);
        b->SetVelocity(vB);
        b->SetAngularVelocity(wB);
    }

    bool WheelJoint::SolvePosition()
    {
        Body *a = mBodyA;
        Body *b = mBodyB;

        Transform xfA = a->GetTransform();
        Transform xfB = b->GetTransform();

        glm::vec2 worldAnchorA = xfA.Transform(mLocalAnchorA);
        glm::vec2 worldAnchorB = xfB.Transform(mLocalAnchorB);
        glm::vec2 rA = worldAnchorA - a->WorldCenter();
        glm::vec2 rB = worldAnchorB - b->WorldCenter();
        glm::vec2 d = worldAnchorB - worldAnchorA;

        glm::vec2 ay = Rotate(xfA, mLocalYAxisA);

        float sAy = Cross(d + rA, ay);
        float sBy = Cross(rB, ay);

        float C = Dot(d, ay);

        float invMass = a->InvMass() + b->InvMass() + a->InvI() * mSAy * mSAy + b->InvI() * mSBy * mSBy;

        float impulse = invMass != 0.0f ? -C / invMass : 0.0f;

        glm::vec2 P = impulse * ay;
        float LA = impulse * sAy;
        float LB = impulse * sBy;

        a->ShiftCenter(-a->InvMass() * P, -a->InvI() * LA);
        b->ShiftCenter(b->InvMass() * P, b->InvI() * LB);

        return std::fabs(C) <= kLinearSlop;
    }

} // namespace kx
