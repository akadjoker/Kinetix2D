#pragma once

#include <glm/glm.hpp>

#include "joint.h"

namespace kx
{

    class DistanceJoint : public Joint
    {
    public:
        DistanceJoint(Body *a, Body *b, const glm::vec2 &worldAnchorA, const glm::vec2 &worldAnchorB);

        void SetSpring(float frequencyHz, float dampingRatio);
        void SetLengthRange(float minLength, float maxLength);

        glm::vec2 AnchorA() const override;
        glm::vec2 AnchorB() const override;

    protected:
        void InitVelocity(float dt) override;
        void SolveVelocity(float dt) override;
        bool SolvePosition() override;

    private:
        glm::vec2 mLocalAnchorA;
        glm::vec2 mLocalAnchorB;
        float mLength;
        float mMinLength;
        float mMaxLength;

        float mStiffness;
        float mDamping;

        float mGamma;
        float mBias;
        float mImpulse;
        float mLowerImpulse;
        float mUpperImpulse;

        glm::vec2 mU;
        glm::vec2 mRA, mRB;
        float mCurrentLength;
        float mMass;
        float mSoftMass;
    };

} // namespace kx
