#pragma once

#include "k2d/Joint2D.h"

namespace k2d
{

    class RevoluteJoint2D : public Joint2D
    {
    public:
        RevoluteJoint2D();

        const Math::Vec2 &localAnchorA() const { return mLocalAnchorA; }
        void setLocalAnchorA(const Math::Vec2 &anchor) { mLocalAnchorA = anchor; }
        const Math::Vec2 &localAnchorB() const { return mLocalAnchorB; }
        void setLocalAnchorB(const Math::Vec2 &anchor) { mLocalAnchorB = anchor; }

        // Unset means onConnected() derives anchor B from the authored placement.
        bool anchorsConfigured() const { return mAnchorsConfigured; }
        void setAnchorsConfigured(bool configured) { mAnchorsConfigured = configured; }

        float referenceAngle() const { return mReferenceAngle; }
        void setReferenceAngle(float angle) { mReferenceAngle = angle; }

        bool motorEnabled() const { return mEnableMotor; }
        float motorSpeed() const { return mMotorSpeed; }
        float maxMotorTorque() const { return mMaxMotorTorque; }
        void setMotor(bool enabled, float speed, float maxTorque);

        bool limitEnabled() const { return mEnableLimit; }
        float lowerAngle() const { return mLowerAngle; }
        float upperAngle() const { return mUpperAngle; }
        void setLimits(bool enabled, float lowerRad, float upperRad);

        Math::Vec2 anchorA() const override;
        Math::Vec2 anchorB() const override;

    protected:
        void onConnected() override;

        void initVelocity(float dt) override;
        void solveVelocity(float dt) override;
        bool solvePosition() override;

    private:
        friend class GearJoint2D;

        Math::Vec2 mLocalAnchorA;
        Math::Vec2 mLocalAnchorB;
        bool mAnchorsConfigured;
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
    };

    template <> struct ComponentMatch<RevoluteJoint2D>
    {
        static bool test(const Component *component)
        {
            return dynamic_cast<const RevoluteJoint2D *>(component) != nullptr;
        }
    };

}
