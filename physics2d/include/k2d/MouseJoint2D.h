#pragma once

#include "k2d/Joint2D.h"

namespace k2d
{

    // Pulls the owner's RigidBody2D toward a moving world-space target point.
    // Has no body A (the target is the "world"), so it needs no target name.
    class MouseJoint2D : public Joint2D
    {
    public:
        MouseJoint2D();

        const Math::Vec2 &target() const { return mTarget; }
        void setTarget(const Math::Vec2 &target) { mTarget = target; }

        float maxForce() const { return mMaxForce; }
        void setMaxForce(float force) { mMaxForce = force > 0.0f ? force : 0.0f; }

        float springFrequency() const { return mSpringFrequency; }
        float springDamping() const { return mSpringDamping; }
        void setSpring(float frequencyHz, float dampingRatio);

        Math::Vec2 anchorA() const override { return mTarget; }
        Math::Vec2 anchorB() const override;

    protected:
        void resolve() override;
        void initVelocity(float dt) override;
        void solveVelocity(float dt) override;

    private:
        Math::Vec2 mTarget;
        Math::Vec2 mLocalAnchor;
        float mMaxForce;
        float mSpringFrequency;
        float mSpringDamping;
        float mStiffness;
        float mDamping;

        Math::Vec2 mRB;
        Math::Vec2 mC;
        Math::Vec2 mImpulse;
        float mGamma;
        float mMass00, mMass01, mMass10, mMass11;
    };

    template <> struct ComponentMatch<MouseJoint2D>
    {
        static bool test(const Component *component)
        {
            return dynamic_cast<const MouseJoint2D *>(component) != nullptr;
        }
    };

}
