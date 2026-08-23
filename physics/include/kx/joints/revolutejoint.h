#pragma once

#include <glm/glm.hpp>

#include "joint.h"

namespace kx
{

    class RevoluteJoint : public Joint
    {
    public:
        RevoluteJoint(Body *a, Body *b, const glm::vec2 &worldAnchor);

        void SetMotor(bool enabled, float speed, float maxTorque);
        void SetLimits(bool enabled, float lowerRad, float upperRad);

        glm::vec2 AnchorA() const override;
        glm::vec2 AnchorB() const override;

    protected:
        void InitVelocity(float dt) override;
        void SolveVelocity(float dt) override;
        bool SolvePosition() override;

    private:
        glm::vec2 mLocalAnchorA;
        glm::vec2 mLocalAnchorB;
        float mReferenceAngle;

        glm::vec2 mImpulse;
        float mMotorImpulse;
        float mLowerImpulse;
        float mUpperImpulse;

        bool mEnableMotor;
        float mMaxMotorTorque;
        float mMotorSpeed;

        bool mEnableLimit;
        float mLowerAngle;
        float mUpperAngle;

        glm::vec2 mRA, mRB;
        float mK11, mK12, mK22;
        float mAngle;
        float mAxialMass;

        friend class GearJoint;
    };

} 