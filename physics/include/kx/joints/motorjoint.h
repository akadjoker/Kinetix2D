#pragma once

#include <mathc.h>

#include "joint.h"

namespace kx
{

    class MotorJoint : public Joint
    {
    public:
        MotorJoint(Body *a, Body *b);

        void SetLinearOffset(const Math::Vec2 &offset);
        const Math::Vec2 &LinearOffset() const { return mLinearOffset; }

        void SetAngularOffset(float offset);
        float AngularOffset() const { return mAngularOffset; }

        void SetMaxForce(float force);
        float MaxForce() const { return mMaxForce; }

        void SetMaxTorque(float torque);
        float MaxTorque() const { return mMaxTorque; }

        void SetCorrectionFactor(float factor);
        float CorrectionFactor() const { return mCorrectionFactor; }

        Math::Vec2 AnchorA() const override;
        Math::Vec2 AnchorB() const override;

    protected:
        void InitVelocity(float dt) override;
        void SolveVelocity(float dt) override;

    private:
        Math::Vec2 mLinearOffset;
        float mAngularOffset;
        Math::Vec2 mLinearImpulse;
        float mAngularImpulse;
        float mMaxForce;
        float mMaxTorque;
        float mCorrectionFactor;

        Math::Vec2 mRA;
        Math::Vec2 mRB;
        Math::Vec2 mLinearError;
        float mAngularError;
        float mLinearMass11;
        float mLinearMass12;
        float mLinearMass22;
        float mAngularMass;
    };

} 