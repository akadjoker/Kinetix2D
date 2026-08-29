#pragma once

#include "k2d/Steering2D.h"

namespace k2d
{

    class Seek2D : public Steering2D
    {
    public:
        Seek2D();

        Math::Vec2 force(float deltaTime, const Math::Vec2 &position, const Math::Vec2 &velocity) const override;
    };

    template <> struct ComponentMatch<Seek2D>
    {
        static bool test(const Component *component)
        {
            return dynamic_cast<const Seek2D *>(component) != nullptr;
        }
    };

}
