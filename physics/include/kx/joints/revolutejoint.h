#pragma once

#include <mathc.h>

#include "joint.h"

namespace kx
{

    class RevoluteJoint : public Joint
    {
    public:
        RevoluteJoint(Body *a, Body *b, const Math::Vec2 &worldAnchor);

        void SetMotor(bool enabled, float speed, float maxTorque);
        void SetLimits(bool enabled, float lowerRad, float upperRad);

        Math::Vec2 AnchorA() const override;
        Math::Vec2 AnchorB() const override;

    protected:
        void InitVelocity(float dt) override;
        void SolveVelocity(float dt) override;
        bool SolvePosition() override;

    private:
        Math::Vec2 mLocalAnchorA;
        Math::Vec2 mLocalAnchorB;
        float mReferenceAngle;

        Math::Vec2 mImpulse;
        float mMotorImpulse;
        float mLowerImpulse;
        float mUpperImpulse;

        bool mEnableMotor;
        float mMaxMotorTorque;
        float mMotorSpeed;

        bool mEnableLimit;
        float mLowerAngle;
        float mUpperAngle;

        Math::Vec2 mRA, mRB;
        float mK11, mK12, mK22;
        float mAngle;
        float mAxialMass;

        friend class GearJoint;
    };

} 