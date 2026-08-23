#pragma once

#include <mathc.h>

#include "joint.h"

namespace kx
{

    class RevoluteJoint;

    class GearJoint : public Joint
    {
    public:
        GearJoint(RevoluteJoint *jointA, RevoluteJoint *jointB, float ratio);

        void SetRatio(float ratio) { mRatio = ratio; }
        float Ratio() const { return mRatio; }

        Math::Vec2 AnchorA() const override;
        Math::Vec2 AnchorB() const override;

    protected:
        void InitVelocity(float dt) override;
        void SolveVelocity(float dt) override;
        bool SolvePosition() override;

        bool DependsOnBody(const Body *body) const override
        {
            return body == mBodyC || body == mBodyD;
        }
        bool DependsOnJoint(const Joint *joint) const override;

    private:
        RevoluteJoint *mJoint1;
        RevoluteJoint *mJoint2;

        Body *mBodyC;
        Body *mBodyD;

        Math::Vec2 mLocalAnchorA;
        Math::Vec2 mLocalAnchorB;

        float mReferenceAngleA;
        float mReferenceAngleB;

        float mConstant;
        float mRatio;
        float mImpulse;
        float mMass;
    };

} 