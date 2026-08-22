#pragma once

#include <glm/glm.hpp>

#include "joint.h"

namespace kx
{

    class MotorJoint : public Joint
    {
    public:
        MotorJoint(Body *a, Body *b);

        void SetLinearOffset(const glm::vec2 &offset);
        const glm::vec2 &LinearOffset() const { return mLinearOffset; }

        void SetAngularOffset(float offset);
        float AngularOffset() const { return mAngularOffset; }

        void SetMaxForce(float force);
        float MaxForce() const { return mMaxForce; }

        void SetMaxTorque(float torque);
        float MaxTorque() const { return mMaxTorque; }

        void SetCorrectionFactor(float factor);
        float CorrectionFactor() const { return mCorrectionFactor; }

        glm::vec2 AnchorA() const override;
        glm::vec2 AnchorB() const override;

    protected:
        void InitVelocity(float dt) override;
        void SolveVelocity(float dt) override;

    private:
        glm::vec2 mLinearOffset;
        float mAngularOffset;
        glm::vec2 mLinearImpulse;
        float mAngularImpulse;
        float mMaxForce;
        float mMaxTorque;
        float mCorrectionFactor;

        glm::vec2 mRA;
        glm::vec2 mRB;
        glm::vec2 mLinearError;
        float mAngularError;
        float mLinearMass11;
        float mLinearMass12;
        float mLinearMass22;
        float mAngularMass;
    };

} // namespace kx
