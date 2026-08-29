#pragma once

#include "k2d/Joint2D.h"

namespace k2d
{

    class WheelJoint2D : public Joint2D
    {
    public:
        WheelJoint2D();

        const Math::Vec2 &localAnchorA() const { return mLocalAnchorA; }
        void setLocalAnchorA(const Math::Vec2 &anchor)
        {
            mLocalAnchorA = anchor;
            mAnchorsConfigured = true;
        }
        const Math::Vec2 &localAnchorB() const { return mLocalAnchorB; }
        void setLocalAnchorB(const Math::Vec2 &anchor)
        {
            mLocalAnchorB = anchor;
            mAnchorsConfigured = true;
        }
        bool anchorsConfigured() const { return mAnchorsConfigured; }
        void setAnchorsConfigured(bool configured) { mAnchorsConfigured = configured; }
        const Math::Vec2 &localAxisA() const { return mLocalXAxisA; }
        void setLocalAxisA(const Math::Vec2 &axis);

        bool motorEnabled() const { return mEnableMotor; }
        float motorSpeed() const { return mMotorSpeed; }
        float maxMotorTorque() const { return mMaxMotorTorque; }
        void setMotor(bool enabled, float speed, float maxTorque);

        float springFrequency() const { return mSpringFrequency; }
        float springDamping() const { return mSpringDamping; }
        void setSpring(float frequencyHz, float dampingRatio);

        Math::Vec2 anchorA() const override;
        Math::Vec2 anchorB() const override;

    protected:
        void onConnected() override;
        void initVelocity(float dt) override;
        void solveVelocity(float dt) override;
        bool solvePosition() override;

    private:
        Math::Vec2 mLocalAnchorA;
        Math::Vec2 mLocalAnchorB;
        // Unset means onConnected() derives anchor B from the authored placement.
        bool mAnchorsConfigured;
        Math::Vec2 mLocalXAxisA;
        Math::Vec2 mLocalYAxisA;

        float mImpulse;
        float mMotorImpulse;
        float mSpringImpulse;

        float mMaxMotorTorque;
        float mMotorSpeed;
        bool mEnableMotor;

        float mSpringFrequency;
        float mSpringDamping;
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

    template <> struct ComponentMatch<WheelJoint2D>
    {
        static bool test(const Component *component)
        {
            return dynamic_cast<const WheelJoint2D *>(component) != nullptr;
        }
    };

}
