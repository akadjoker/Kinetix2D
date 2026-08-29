#pragma once

#include "k2d/Joint2D.h"

namespace k2d
{

    class MotorJoint2D : public Joint2D
    {
    public:
        MotorJoint2D();

        const Math::Vec2 &linearOffset() const { return mLinearOffset; }
        void setLinearOffset(const Math::Vec2 &offset);
        float angularOffset() const { return mAngularOffset; }
        void setAngularOffset(float offset);
        float maxForce() const { return mMaxForce; }
        void setMaxForce(float force);
        float maxTorque() const { return mMaxTorque; }
        void setMaxTorque(float torque);
        float correctionFactor() const { return mCorrectionFactor; }
        void setCorrectionFactor(float factor);

        Math::Vec2 anchorA() const override;
        Math::Vec2 anchorB() const override;

    protected:
        void initVelocity(float dt) override;
        void solveVelocity(float dt) override;

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

    template <> struct ComponentMatch<MotorJoint2D>
    {
        static bool test(const Component *component)
        {
            return dynamic_cast<const MotorJoint2D *>(component) != nullptr;
        }
    };

}
