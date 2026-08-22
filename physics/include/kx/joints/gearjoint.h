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

        // A GearJoint reaches past its own BodyA()/BodyB() into the two anchor
        // bodies of its underlying revolute joints, and into the revolute joints
        // themselves — World needs to know about both to cascade-destroy safely.
        // Defined in gearjoint.cpp: casting RevoluteJoint* to Joint* needs the
        // complete RevoluteJoint type, which this header only forward-declares.
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
