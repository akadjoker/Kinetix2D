#pragma once

#include "k2d/Steering2D.h"

namespace k2d
{

    class Flee2D : public Steering2D
    {
    public:
        Flee2D();

        // Zero or less means no limit: the threat is fled from at any distance.
        float radius() const { return mRadius; }
        void setRadius(float radius);

        Math::Vec2 force(float deltaTime, const Math::Vec2 &position, const Math::Vec2 &velocity) const override;

    private:
        float mRadius;
    };

    template <> struct ComponentMatch<Flee2D>
    {
        static bool test(const Component *component)
        {
            return dynamic_cast<const Flee2D *>(component) != nullptr;
        }
    };

}
