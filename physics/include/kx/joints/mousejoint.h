#pragma once

#include <mathc.h>

#include "joint.h"

namespace kx
{

    class MouseJoint : public Joint
    {
    public:
        MouseJoint(Body *body, const Math::Vec2 &target, float maxForce,
                   float frequencyHz = 5.0f, float dampingRatio = 0.7f);

        void SetTarget(const Math::Vec2 &target);
        const Math::Vec2 &Target() const { return mTarget; }

        Math::Vec2 AnchorA() const override { return mTarget; }
        Math::Vec2 AnchorB() const override;

    protected:
        void InitVelocity(float dt) override;
        void SolveVelocity(float dt) override;

    private:
        Math::Vec2 mTarget;
        Math::Vec2 mLocalAnchor;
        float mMaxForce;
        float mStiffness;
        float mDamping;

        Math::Vec2 mRB;
        Math::Vec2 mC;
        Math::Vec2 mImpulse;
        float mGamma;
        float mMass00, mMass01, mMass10, mMass11;
    };

} 