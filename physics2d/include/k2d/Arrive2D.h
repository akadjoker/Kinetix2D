#pragma once

#include "k2d/Steering2D.h"

namespace k2d
{

    class Arrive2D : public Steering2D
    {
    public:
        Arrive2D();

        float slowRadius() const { return mSlowRadius; }
        void setSlowRadius(float radius);
        float stopRadius() const { return mStopRadius; }
        void setStopRadius(float radius);

        Math::Vec2 force(float deltaTime, const Math::Vec2 &position, const Math::Vec2 &velocity) const override;

    private:
        float mSlowRadius;
        float mStopRadius;
    };

    template <> struct ComponentMatch<Arrive2D>
    {
        static bool test(const Component *component)
        {
            return dynamic_cast<const Arrive2D *>(component) != nullptr;
        }
    };

}
