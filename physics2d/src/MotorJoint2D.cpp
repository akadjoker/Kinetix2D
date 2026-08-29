#include "k2d/MotorJoint2D.h"

#include "k2d/RigidBody2D.h"

#include <cmath>

namespace k2d
{

MotorJoint2D::MotorJoint2D()
    : mLinearOffset(0.0f, 0.0f), mAngularOffset(0.0f), mLinearImpulse(0.0f, 0.0f), mAngularImpulse(0.0f),
      mMaxForce(1.0f), mMaxTorque(1.0f), mCorrectionFactor(0.3f), mRA(0.0f, 0.0f), mRB(0.0f, 0.0f),
      mLinearError(0.0f, 0.0f), mAngularError(0.0f), mLinearMass11(0.0f), mLinearMass12(0.0f), mLinearMass22(0.0f),
      mAngularMass(0.0f)
{
}

void MotorJoint2D::setLinearOffset(const Math::Vec2 &offset)
{
    if (offset != mLinearOffset)
    {
        mLinearOffset = offset;
        if (mBodyA)
            mBodyA->SetAwake(true);
        if (mBodyB)
            mBodyB->SetAwake(true);
    }
}

void MotorJoint2D::setAngularOffset(float offset)
{
    if (offset != mAngularOffset)
    {
        mAngularOffset = offset;
        if (mBodyA)
            mBodyA->SetAwake(true);
        if (mBodyB)
            mBodyB->SetAwake(true);
    }
}

void MotorJoint2D::setMaxForce(float force)
{
    mMaxForce = force > 0.0f ? force : 0.0f;
}

void MotorJoint2D::setMaxTorque(float torque)
{
    mMaxTorque = torque > 0.0f ? torque : 0.0f;
}

void MotorJoint2D::setCorrectionFactor(float factor)
{
    mCorrectionFactor = Clamp(factor, 0.0f, 1.0f);
}

Math::Vec2 MotorJoint2D::anchorA() const
{
    return mBodyA->Position();
}

Math::Vec2 MotorJoint2D::anchorB() const
{
    return mBodyB->Position();
}

void MotorJoint2D::initVelocity(float dt)
{
    (void)dt;

    RigidBody2D *a = mBodyA;
    RigidBody2D *b = mBodyB;
    Transform xfA = a->GetTransform();

    Math::Vec2 target = xfA.Transform(mLinearOffset);
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

    Math::Vec2 impulse = mLinearImpulse;
    a->setVelocity(a->velocity() - mA * impulse);
    a->SetAngularVelocityRadians(a->AngularVelocityRadians() - iA * (Cross(mRA, impulse) + mAngularImpulse));
    b->setVelocity(b->velocity() + mB * impulse);
    b->SetAngularVelocityRadians(b->AngularVelocityRadians() + iB * (Cross(mRB, impulse) + mAngularImpulse));
}

void MotorJoint2D::solveVelocity(float dt)
{
    RigidBody2D *a = mBodyA;
    RigidBody2D *b = mBodyB;
    float mA = a->InvMass();
    float mB = b->InvMass();
    float iA = a->InvI();
    float iB = b->InvI();
    float invDt = dt > 0.0f ? 1.0f / dt : 0.0f;

    Math::Vec2 vA = a->velocity();
    float wA = a->AngularVelocityRadians();
    Math::Vec2 vB = b->velocity();
    float wB = b->AngularVelocityRadians();

    float angularCdot = wB - wA + invDt * mCorrectionFactor * mAngularError;
    float angularImpulse = -mAngularMass * angularCdot;
    float oldAngularImpulse = mAngularImpulse;
    float maxAngularImpulse = dt * mMaxTorque;
    mAngularImpulse = Clamp(mAngularImpulse + angularImpulse, -maxAngularImpulse, maxAngularImpulse);
    angularImpulse = mAngularImpulse - oldAngularImpulse;
    wA -= iA * angularImpulse;
    wB += iB * angularImpulse;

    Math::Vec2 linearCdot = vB + Cross(wB, mRB) - vA - Cross(wA, mRA) + invDt * mCorrectionFactor * mLinearError;
    Math::Vec2 linearImpulse(-(mLinearMass11 * linearCdot.x + mLinearMass12 * linearCdot.y),
                            -(mLinearMass12 * linearCdot.x + mLinearMass22 * linearCdot.y));
    Math::Vec2 oldLinearImpulse = mLinearImpulse;
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

    a->setVelocity(vA);
    a->SetAngularVelocityRadians(wA);
    b->setVelocity(vB);
    b->SetAngularVelocityRadians(wB);
}

}
