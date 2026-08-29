#pragma once

#include "k2d/Joint2D.h"
#include "k2d/RevoluteJoint2D.h"

#include <ct/string.hpp>

namespace k2d
{

    // Couples two RevoluteJoint2D hinges by name + joint index (a GameObject
    // may carry more than one joint, disambiguated like colliders). Resolved
    // once on attach; both referenced joints must already be attached by then.
    class GearJoint2D : public Joint2D
    {
    public:
        GearJoint2D();

        void setJointA(const char *objectName, int jointIndex);
        const ct::String &jointATargetName() const { return mJoint1TargetName; }
        int jointAIndex() const { return mJoint1Index; }

        void setJointB(const char *objectName, int jointIndex);
        const ct::String &jointBTargetName() const { return mJoint2TargetName; }
        int jointBIndex() const { return mJoint2Index; }

        float ratio() const { return mRatio; }
        void setRatio(float ratio) { mRatio = ratio; }

        Math::Vec2 anchorA() const override;
        Math::Vec2 anchorB() const override;

    protected:
        void resolve() override;
        void initVelocity(float dt) override;
        void solveVelocity(float dt) override;
        bool solvePosition() override;

        bool dependsOnBody(const RigidBody2D *body) const override
        {
            return body == mBodyC || body == mBodyD;
        }
        bool dependsOnJoint(const Joint2D *joint) const override
        {
            return joint == static_cast<const Joint2D *>(mJoint1) || joint == static_cast<const Joint2D *>(mJoint2);
        }
        void invalidate() override
        {
            mBodyA = mBodyB = nullptr;
            mBodyC = mBodyD = nullptr;
            mJoint1 = nullptr;
            mJoint2 = nullptr;
        }

    private:
        ct::String mJoint1TargetName;
        int mJoint1Index;
        ct::String mJoint2TargetName;
        int mJoint2Index;

        RevoluteJoint2D *mJoint1;
        RevoluteJoint2D *mJoint2;
        RigidBody2D *mBodyC;
        RigidBody2D *mBodyD;

        Math::Vec2 mLocalAnchorA;
        Math::Vec2 mLocalAnchorB;
        float mReferenceAngleA;
        float mReferenceAngleB;

        float mConstant;
        float mRatio;
        float mImpulse;
        float mMass;
    };

    template <> struct ComponentMatch<GearJoint2D>
    {
        static bool test(const Component *component)
        {
            return dynamic_cast<const GearJoint2D *>(component) != nullptr;
        }
    };

}
