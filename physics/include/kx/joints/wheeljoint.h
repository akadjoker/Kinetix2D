#pragma once

#include <glm/glm.hpp>

#include "joint.h"

namespace kx
{

    class WheelJoint : public Joint
    {
    public:
        WheelJoint(Body *chassis, Body *wheel, const glm::vec2 &worldAnchor, const glm::vec2 &worldAxis,
                  float frequencyHz = 4.0f, float dampingRatio = 0.7f);

        void SetMotor(bool enabled, float speed, float maxTorque);
        void SetSpring(float frequencyHz, float dampingRatio);

        glm::vec2 AnchorA() const override;
        glm::vec2 AnchorB() const override;

    protected:
        void InitVelocity(float dt) override;
        void SolveVelocity(float dt) override;
        bool SolvePosition() override;

    private:
        glm::vec2 mLocalAnchorA;
        glm::vec2 mLocalAnchorB;
        glm::vec2 mLocalXAxisA;
        glm::vec2 mLocalYAxisA;

        float mImpulse;
        float mMotorImpulse;
        float mSpringImpulse;

        float mMaxMotorTorque;
        float mMotorSpeed;
        bool mEnableMotor;

        float mStiffness;
        float mDamping;

        glm::vec2 mAx, mAy;
        float mSAx, mSBx;
        float mSAy, mSBy;

        float mMass;
        float mMotorMass;
        float mAxialMass;
        float mSpringMass;

        float mBias;
        float mGamma;
    };

} // namespace kx
