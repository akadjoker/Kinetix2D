#pragma once

#include "k2d/Steering2D.h"

namespace k2d
{

    class Wander2D : public Steering2D
    {
    public:
        Wander2D();

        float radius() const { return mRadius; }
        void setRadius(float radius);
        float distance() const { return mDistance; }
        void setDistance(float distance);
        float jitter() const { return mJitter; }
        void setJitter(float jitter);

        float angle() const { return mAngle; }

        Math::Vec2 force(float deltaTime, const Math::Vec2 &position, const Math::Vec2 &velocity) const override;

    private:
        float mRadius;
        float mDistance;
        float mJitter;
        mutable float mAngle;
        mutable uint32_t mRandom;
    };

    template <> struct ComponentMatch<Wander2D>
    {
        static bool test(const Component *component)
        {
            return dynamic_cast<const Wander2D *>(component) != nullptr;
        }
    };

}
