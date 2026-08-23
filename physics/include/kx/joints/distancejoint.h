#pragma once

#include <mathc.h>

#include "joint.h"

namespace kx
{

    class DistanceJoint : public Joint
    {
    public:
        DistanceJoint(Body *a, Body *b, const Math::Vec2 &worldAnchorA, const Math::Vec2 &worldAnchorB);

        void SetSpring(float frequencyHz, float dampingRatio);
        void SetLengthRange(float minLength, float maxLength);

        Math::Vec2 AnchorA() const override;
        Math::Vec2 AnchorB() const override;

    protected:
        void InitVelocity(float dt) override;
        void SolveVelocity(float dt) override;
        bool SolvePosition() override;

    private:
        Math::Vec2 mLocalAnchorA;
        Math::Vec2 mLocalAnchorB;
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

        Math::Vec2 mU;
        Math::Vec2 mRA, mRB;
        float mCurrentLength;
        float mMass;
        float mSoftMass;
    };

} 