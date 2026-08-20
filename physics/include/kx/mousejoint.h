#pragma once

#include <glm/glm.hpp>

#include "joint.h"

namespace kx
{

    class MouseJoint : public Joint
    {
    public:
        MouseJoint(Body *body, const glm::vec2 &target, float maxForce,
                   float frequencyHz = 5.0f, float dampingRatio = 0.7f);

        void SetTarget(const glm::vec2 &target);
        const glm::vec2 &Target() const { return mTarget; }

    protected:
        void InitVelocity(float dt) override;
        void SolveVelocity(float dt) override;

    private:
        glm::vec2 mTarget;
        glm::vec2 mLocalAnchor;
        float mMaxForce;
        float mStiffness;
        float mDamping;

        glm::vec2 mRB;
        glm::vec2 mC;
        glm::vec2 mImpulse;
        float mGamma;
        float mMass00, mMass01, mMass10, mMass11;
    };

} // namespace kx
