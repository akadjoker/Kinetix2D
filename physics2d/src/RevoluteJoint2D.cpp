#include "k2d/RevoluteJoint2D.h"

#include "k2d/RigidBody2D.h"

#include <cmath>

namespace k2d
{

RevoluteJoint2D::RevoluteJoint2D()
    : mLocalAnchorA(0.0f, 0.0f), mLocalAnchorB(0.0f, 0.0f), mReferenceAngle(0.0f), mImpulse(0.0f, 0.0f),
      mMotorImpulse(0.0f), mLowerImpulse(0.0f), mUpperImpulse(0.0f), mEnableMotor(false), mMaxMotorTorque(0.0f),
      mMotorSpeed(0.0f), mEnableLimit(false), mLowerAngle(0.0f), mUpperAngle(0.0f), mRA(0.0f, 0.0f),
      mRB(0.0f, 0.0f), mK11(0.0f), mK12(0.0f), mK22(0.0f), mAngle(0.0f), mAxialMass(0.0f)
{
}

void RevoluteJoint2D::setMotor(bool enabled, float speed, float maxTorque)
{
    mEnableMotor = enabled;
    mMotorSpeed = speed;
    mMaxMotorTorque = maxTorque;
}

void RevoluteJoint2D::setLimits(bool enabled, float lowerRad, float upperRad)
{
    mEnableLimit = enabled;
    mLowerAngle = lowerRad;
    mUpperAngle = upperRad;
    mLowerImpulse = 0.0f;
    mUpperImpulse = 0.0f;
}

Math::Vec2 RevoluteJoint2D::anchorA() const
{
    return mBodyA->GetTransform().Transform(mLocalAnchorA);
}

Math::Vec2 RevoluteJoint2D::anchorB() const
{
    return mBodyB->GetTransform().Transform(mLocalAnchorB);
}

void RevoluteJoint2D::initVelocity(float dt)
{
    (void)dt;

    RigidBody2D *a = mBodyA;
    RigidBody2D *b = mBodyB;

    Transform xfA = a->GetTransform();
    Transform xfB = b->GetTransform();

    Math::Vec2 worldAnchorA = xfA.Transform(mLocalAnchorA);
    Math::Vec2 worldAnchorB = xfB.Transform(mLocalAnchorB);
    mRA = worldAnchorA - a->WorldCenter();
    mRB = worldAnchorB - b->WorldCenter();

    float mA = a->InvMass(), mB = b->InvMass();
    float iA = a->InvI(), iB = b->InvI();

    mK11 = mA + mB + mRA.y * mRA.y * iA + mRB.y * mRB.y * iB;
    mK12 = -mRA.y * mRA.x * iA - mRB.y * mRB.x * iB;
    mK22 = mA + mB + mRA.x * mRA.x * iA + mRB.x * mRB.x * iB;

    mAxialMass = iA + iB;
    bool fixedRotation;
    if (mAxialMass > 0.0f)
    {
        mAxialMass = 1.0f / mAxialMass;
        fixedRotation = false;
    }
    else
    {
        fixedRotation = true;
    }

    mAngle = b->Angle() - a->Angle() - mReferenceAngle;
    if (!mEnableLimit || fixedRotation)
    {
        mLowerImpulse = 0.0f;
        mUpperImpulse = 0.0f;
    }

    if (!mEnableMotor || fixedRotation)
        mMotorImpulse = 0.0f;

    float axialImpulse = mMotorImpulse + mLowerImpulse - mUpperImpulse;
    Math::Vec2 P = mImpulse;

    a->setVelocity(a->velocity() - mA * P);
    a->SetAngularVelocityRadians(a->AngularVelocityRadians() - iA * (Cross(mRA, P) + axialImpulse));
    b->setVelocity(b->velocity() + mB * P);
    b->SetAngularVelocityRadians(b->AngularVelocityRadians() + iB * (Cross(mRB, P) + axialImpulse));
}

void RevoluteJoint2D::solveVelocity(float dt)
{
    RigidBody2D *a = mBodyA;
    RigidBody2D *b = mBodyB;

    float mA = a->InvMass(), mB = b->InvMass();
    float iA = a->InvI(), iB = b->InvI();

    Math::Vec2 vA = a->velocity();
    float wA = a->AngularVelocityRadians();
    Math::Vec2 vB = b->velocity();
    float wB = b->AngularVelocityRadians();

    bool fixedRotation = (iA + iB == 0.0f);

    if (mEnableMotor && !fixedRotation)
    {
        float Cdot = wB - wA - mMotorSpeed;
        float impulse = -mAxialMass * Cdot;
        float oldImpulse = mMotorImpulse;
        float maxImpulse = dt * mMaxMotorTorque;
        mMotorImpulse = Clamp(mMotorImpulse + impulse, -maxImpulse, maxImpulse);
        impulse = mMotorImpulse - oldImpulse;

        wA -= iA * impulse;
        wB += iB * impulse;
    }

    if (mEnableLimit && !fixedRotation)
    {
        {
            float C = mAngle - mLowerAngle;
            float Cdot = wB - wA;
            float impulse = -mAxialMass * (Cdot + (C > 0.0f ? C : 0.0f) / dt);
            float oldImpulse = mLowerImpulse;
            mLowerImpulse = mLowerImpulse + impulse > 0.0f ? mLowerImpulse + impulse : 0.0f;
            impulse = mLowerImpulse - oldImpulse;

            wA -= iA * impulse;
            wB += iB * impulse;
        }

        {
            float C = mUpperAngle - mAngle;
            float Cdot = wA - wB;
            float impulse = -mAxialMass * (Cdot + (C > 0.0f ? C : 0.0f) / dt);
            float oldImpulse = mUpperImpulse;
            mUpperImpulse = mUpperImpulse + impulse > 0.0f ? mUpperImpulse + impulse : 0.0f;
            impulse = mUpperImpulse - oldImpulse;

            wA += iA * impulse;
            wB -= iB * impulse;
        }
    }

    {
        Math::Vec2 Cdot = vB + Cross(wB, mRB) - vA - Cross(wA, mRA);
        Math::Vec2 rhs = -Cdot;

        float det = mK11 * mK22 - mK12 * mK12;
        float invDet = det != 0.0f ? 1.0f / det : 0.0f;
        Math::Vec2 impulse(invDet * (mK22 * rhs.x - mK12 * rhs.y), invDet * (mK11 * rhs.y - mK12 * rhs.x));

        mImpulse += impulse;

        vA -= mA * impulse;
        wA -= iA * Cross(mRA, impulse);
        vB += mB * impulse;
        wB += iB * Cross(mRB, impulse);
    }

    a->setVelocity(vA);
    a->SetAngularVelocityRadians(wA);
    b->setVelocity(vB);
    b->SetAngularVelocityRadians(wB);
}

bool RevoluteJoint2D::solvePosition()
{
    RigidBody2D *a = mBodyA;
    RigidBody2D *b = mBodyB;

    float angularError = 0.0f;
    float positionError = 0.0f;

    bool fixedRotation = (a->InvI() + b->InvI() == 0.0f);

    if (mEnableLimit && !fixedRotation)
    {
        float angle = b->Angle() - a->Angle() - mReferenceAngle;
        float C = 0.0f;

        if (std::fabs(mUpperAngle - mLowerAngle) < 2.0f * kAngularSlop)
            C = Clamp(angle - mLowerAngle, -kMaxAngularCorrection, kMaxAngularCorrection);
        else if (angle <= mLowerAngle)
            C = Clamp(angle - mLowerAngle + kAngularSlop, -kMaxAngularCorrection, 0.0f);
        else if (angle >= mUpperAngle)
            C = Clamp(angle - mUpperAngle - kAngularSlop, 0.0f, kMaxAngularCorrection);

        float limitImpulse = -mAxialMass * C;
        a->ShiftCenter(Math::Vec2(0.0f, 0.0f), -a->InvI() * limitImpulse);
        b->ShiftCenter(Math::Vec2(0.0f, 0.0f), b->InvI() * limitImpulse);
        angularError = std::fabs(C);
    }

    {
        Transform xfA = a->GetTransform();
        Transform xfB = b->GetTransform();
        Math::Vec2 worldAnchorA = xfA.Transform(mLocalAnchorA);
        Math::Vec2 worldAnchorB = xfB.Transform(mLocalAnchorB);
        Math::Vec2 rA = worldAnchorA - a->WorldCenter();
        Math::Vec2 rB = worldAnchorB - b->WorldCenter();

        Math::Vec2 C = worldAnchorB - worldAnchorA;
        positionError = std::sqrt(Dot(C, C));

        float mA = a->InvMass(), mB = b->InvMass();
        float iA = a->InvI(), iB = b->InvI();

        float k11 = mA + mB + iA * rA.y * rA.y + iB * rB.y * rB.y;
        float k12 = -iA * rA.x * rA.y - iB * rB.x * rB.y;
        float k22 = mA + mB + iA * rA.x * rA.x + iB * rB.x * rB.x;

        float det = k11 * k22 - k12 * k12;
        float invDet = det != 0.0f ? 1.0f / det : 0.0f;
        Math::Vec2 impulse(-invDet * (k22 * C.x - k12 * C.y), -invDet * (k11 * C.y - k12 * C.x));

        a->ShiftCenter(-mA * impulse, -iA * Cross(rA, impulse));
        b->ShiftCenter(mB * impulse, iB * Cross(rB, impulse));
    }

    return positionError <= kLinearSlop && angularError <= kAngularSlop;
}

}
