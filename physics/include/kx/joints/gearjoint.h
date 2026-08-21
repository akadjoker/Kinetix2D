#pragma once

#include <glm/glm.hpp>

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

        glm::vec2 AnchorA() const override;
        glm::vec2 AnchorB() const override;

    protected:
        void InitVelocity(float dt) override;
        void SolveVelocity(float dt) override;
        bool SolvePosition() override;

    private:
        RevoluteJoint *mJoint1;
        RevoluteJoint *mJoint2;

        Body *mBodyC;
        Body *mBodyD;

        glm::vec2 mLocalAnchorA;
        glm::vec2 mLocalAnchorB;

        float mReferenceAngleA;
        float mReferenceAngleB;

        float mConstant;
        float mRatio;
        float mImpulse;
        float mMass;
    };

} // namespace kx
