#pragma once

#include <mathc.h>

#include "joint.h"

namespace kx
{

    class WheelJoint : public Joint
    {
    public:
        WheelJoint(Body *chassis, Body *wheel, const Math::Vec2 &worldAnchor, const Math::Vec2 &worldAxis,
                  float frequencyHz = 4.0f, float dampingRatio = 0.7f);

        void SetMotor(bool enabled, float speed, float maxTorque);
        void SetSpring(float frequencyHz, float dampingRatio);

        Math::Vec2 AnchorA() const override;
        Math::Vec2 AnchorB() const override;

    protected:
        void InitVelocity(float dt) override;
        void SolveVelocity(float dt) override;
        bool SolvePosition() override;

    private:
        Math::Vec2 mLocalAnchorA;
        Math::Vec2 mLocalAnchorB;
        Math::Vec2 mLocalXAxisA;
        Math::Vec2 mLocalYAxisA;

        float mImpulse;
        float mMotorImpulse;
        float mSpringImpulse;

        float mMaxMotorTorque;
        float mMotorSpeed;
        bool mEnableMotor;

        float mStiffness;
        float mDamping;

        Math::Vec2 mAx, mAy;
        float mSAx, mSBx;
        float mSAy, mSBy;

        float mMass;
        float mMotorMass;
        float mAxialMass;
        float mSpringMass;

        float mBias;
        float mGamma;
    };

} 