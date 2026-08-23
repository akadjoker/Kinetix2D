#include "kx/joints/motorjoint.h"

#include "kx/body.h"

#include <cmath>

namespace kx
{

    MotorJoint::MotorJoint(Body *a, Body *b)
        : Joint(JointType::Motor, a, b),
          mLinearOffset(InvTransformPoint(a->GetTransform(), b->Position())),
          mAngularOffset(b->Angle() - a->Angle()),
          mLinearImpulse(0.0f), mAngularImpulse(0.0f),
          mMaxForce(1.0f), mMaxTorque(1.0f), mCorrectionFactor(0.3f),
          mRA(0.0f), mRB(0.0f), mLinearError(0.0f), mAngularError(0.0f),
          mLinearMass11(0.0f), mLinearMass12(0.0f), mLinearMass22(0.0f),
          mAngularMass(0.0f)
    {
    }

    void MotorJoint::SetLinearOffset(const glm::vec2 &offset)
    {
        if (offset != mLinearOffset)
        {
            mLinearOffset = offset;
            mBodyA->SetAwake(true);
            mBodyB->SetAwake(true);
        }
    }

    void MotorJoint::SetAngularOffset(float offset)
    {
        if (offset != mAngularOffset)
        {
            mAngularOffset = offset;
            mBodyA->SetAwake(true);
            mBodyB->SetAwake(true);
        }
    }

    void MotorJoint::SetMaxForce(float force)
    {
        mMaxForce = force > 0.0f ? force : 0.0f;
    }

    void MotorJoint::SetMaxTorque(float torque)
    {
        mMaxTorque = torque > 0.0f ? torque : 0.0f;
    }

    void MotorJoint::SetCorrectionFactor(float factor)
    {
        mCorrectionFactor = Clamp(factor, 0.0f, 1.0f);
    }

    glm::vec2 MotorJoint::AnchorA() const
    {
        return mBodyA->Position();
    }

    glm::vec2 MotorJoint::AnchorB() const
    {
        return mBodyB->Position();
    }

    void MotorJoint::InitVelocity(float dt)
    {
        (void)dt;

        Body *a = mBodyA;
        Body *b = mBodyB;
        Transform xfA = a->GetTransform();

        glm::vec2 target = xfA.Transform(mLinearOffset);
        mRA = target - a->WorldCenter();
        mRB = b->Position() - b->WorldCenter();

        float mA = a->InvMass();
        float mB = b->InvMass();
        float iA = a->InvI();
        float iB = b->InvI();

        float k11 = mA + mB + iA * mRA.y * mRA.y + iB * mRB.y * mRB.y;
        float k12 = -iA * mRA.x * mRA.y - iB * mRB.x * mRB.y;
        float k22 = mA + mB + iA * mRA.x * mRA.x + iB * mRB.x * mRB.x;
        float determinant = k11 * k22 - k12 * k12;
        if (determinant != 0.0f)
        {
            float inverse = 1.0f / determinant;
            mLinearMass11 = inverse * k22;
            mLinearMass12 = -inverse * k12;
            mLinearMass22 = inverse * k11;
        }
        else
        {
            mLinearMass11 = 0.0f;
            mLinearMass12 = 0.0f;
            mLinearMass22 = 0.0f;
        }

        mAngularMass = iA + iB;
        if (mAngularMass > 0.0f)
            mAngularMass = 1.0f / mAngularMass;

        mLinearError = b->Position() - target;
        mAngularError = b->Angle() - a->Angle() - mAngularOffset;

        glm::vec2 impulse = mLinearImpulse;
        a->SetVelocity(a->Velocity() - mA * impulse);
        a->SetAngularVelocity(a->AngularVelocity() - iA * (Cross(mRA, impulse) + mAngularImpulse));
        b->SetVelocity(b->Velocity() + mB * impulse);
        b->SetAngularVelocity(b->AngularVelocity() + iB * (Cross(mRB, impulse) + mAngularImpulse));
    }

    void MotorJoint::SolveVelocity(float dt)
    {
        Body *a = mBodyA;
        Body *b = mBodyB;
        float mA = a->InvMass();
        float mB = b->InvMass();
        float iA = a->InvI();
        float iB = b->InvI();
        float invDt = dt > 0.0f ? 1.0f / dt : 0.0f;

        glm::vec2 vA = a->Velocity();
        float wA = a->AngularVelocity();
        glm::vec2 vB = b->Velocity();
        float wB = b->AngularVelocity();

        float angularCdot = wB - wA + invDt * mCorrectionFactor * mAngularError;
        float angularImpulse = -mAngularMass * angularCdot;
        float oldAngularImpulse = mAngularImpulse;
        float maxAngularImpulse = dt * mMaxTorque;
        mAngularImpulse = Clamp(mAngularImpulse + angularImpulse, -maxAngularImpulse, maxAngularImpulse);
        angularImpulse = mAngularImpulse - oldAngularImpulse;
        wA -= iA * angularImpulse;
        wB += iB * angularImpulse;

        glm::vec2 linearCdot = vB + Cross(wB, mRB) - vA - Cross(wA, mRA) +
                               invDt * mCorrectionFactor * mLinearError;
        glm::vec2 linearImpulse(
            -(mLinearMass11 * linearCdot.x + mLinearMass12 * linearCdot.y),
            -(mLinearMass12 * linearCdot.x + mLinearMass22 * linearCdot.y));
        glm::vec2 oldLinearImpulse = mLinearImpulse;
        mLinearImpulse += linearImpulse;

        float maxLinearImpulse = dt * mMaxForce;
        float impulseLengthSquared = Dot(mLinearImpulse, mLinearImpulse);
        if (impulseLengthSquared > maxLinearImpulse * maxLinearImpulse)
        {
            float impulseLength = std::sqrt(impulseLengthSquared);
            mLinearImpulse *= maxLinearImpulse / impulseLength;
        }

        linearImpulse = mLinearImpulse - oldLinearImpulse;
        vA -= mA * linearImpulse;
        wA -= iA * Cross(mRA, linearImpulse);
        vB += mB * linearImpulse;
        wB += iB * Cross(mRB, linearImpulse);

        a->SetVelocity(vA);
        a->SetAngularVelocity(wA);
        b->SetVelocity(vB);
        b->SetAngularVelocity(wB);
    }

} 