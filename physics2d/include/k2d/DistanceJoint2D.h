#pragma once

#include "k2d/Joint2D.h"

namespace k2d
{

    class DistanceJoint2D : public Joint2D
    {
    public:
        DistanceJoint2D();

        const Math::Vec2 &localAnchorA() const { return mLocalAnchorA; }
        void setLocalAnchorA(const Math::Vec2 &anchor) { mLocalAnchorA = anchor; }
        const Math::Vec2 &localAnchorB() const { return mLocalAnchorB; }
        void setLocalAnchorB(const Math::Vec2 &anchor) { mLocalAnchorB = anchor; }

        float length() const { return mLength; }
        void setLength(float length);
        float minLength() const { return mMinLength; }
        float maxLength() const { return mMaxLength; }
        void setLengthRange(float minLength, float maxLength);

        float springFrequency() const { return mSpringFrequency; }
        float springDamping() const { return mSpringDamping; }
        void setSpring(float frequencyHz, float dampingRatio);

        Math::Vec2 anchorA() const override;
        Math::Vec2 anchorB() const override;

    protected:
        void initVelocity(float dt) override;
        void solveVelocity(float dt) override;
        bool solvePosition() override;

    private:
        Math::Vec2 mLocalAnchorA;
        Math::Vec2 mLocalAnchorB;
        float mLength;
        float mMinLength;
        float mMaxLength;
        float mSpringFrequency;
        float mSpringDamping;

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

    template <> struct ComponentMatch<DistanceJoint2D>
    {
        static bool test(const Component *component)
        {
            return dynamic_cast<const DistanceJoint2D *>(component) != nullptr;
        }
    };

}
