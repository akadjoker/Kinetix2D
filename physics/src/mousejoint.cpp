#include "kx/mousejoint.h"

#include "kx/body.h"

#include <cmath>

namespace kx
{

    MouseJoint::MouseJoint(Body *body, const glm::vec2 &target, float maxForce,
                           float frequencyHz, float dampingRatio)
        : Joint(JointType::Mouse, nullptr, body),
          mTarget(target),
          mMaxForce(maxForce),
          mRB(0.0f, 0.0f),
          mC(0.0f, 0.0f),
          mImpulse(0.0f, 0.0f),
          mGamma(0.0f),
          mMass00(0.0f), mMass01(0.0f), mMass10(0.0f), mMass11(0.0f)
    {
        mLocalAnchor = InvTransformPoint(body->GetTransform(), target);

        float mass = body->Mass();
        float omega = 2.0f * kPi * frequencyHz;
        mStiffness = mass * omega * omega;
        mDamping = 2.0f * mass * dampingRatio * omega;
    }

    void MouseJoint::SetTarget(const glm::vec2 &target)
    {
        mTarget = target;
    }

    void MouseJoint::InitVelocity(float dt)
    {
        Body *b = mBodyB;
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
        b->SetAngularVelocity(b->AngularVelocity() * damp);

        b->SetVelocity(b->Velocity() + invMass * mImpulse);
        b->SetAngularVelocity(b->AngularVelocity() + invI * Cross(mRB, mImpulse));
    }

    void MouseJoint::SolveVelocity(float dt)
    {
        Body *b = mBodyB;

        glm::vec2 cdot = b->Velocity() + Cross(b->AngularVelocity(), mRB);
        glm::vec2 rhs = -(cdot + mC + mGamma * mImpulse);
        glm::vec2 impulse(mMass00 * rhs.x + mMass01 * rhs.y,
                          mMass10 * rhs.x + mMass11 * rhs.y);

        glm::vec2 oldImpulse = mImpulse;
        mImpulse += impulse;
        float maxImpulse = dt * mMaxForce;
        float lenSq = mImpulse.x * mImpulse.x + mImpulse.y * mImpulse.y;
        if (lenSq > maxImpulse * maxImpulse)
            mImpulse *= maxImpulse / sqrtf(lenSq);
        impulse = mImpulse - oldImpulse;

        b->SetVelocity(b->Velocity() + b->InvMass() * impulse);
        b->SetAngularVelocity(b->AngularVelocity() + b->InvI() * Cross(mRB, impulse));
    }

} // namespace kx
