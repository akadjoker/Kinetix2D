#pragma once

#include "k2d/Steering2D.h"

namespace k2d
{

    class Separation2D : public Steering2D
    {
    public:
        Separation2D();

        float radius() const { return mRadius; }
        void setRadius(float radius);

        // Which collision categories count as crowd. Neighbours are found in
        // the physics broadphase, so one is only seen through its collider.
        uint16_t mask() const { return mMask; }
        void setMask(uint16_t mask);

        Math::Vec2 force(float deltaTime, const Math::Vec2 &position, const Math::Vec2 &velocity) const override;

    private:
        float mRadius;
        uint16_t mMask;
    };

    template <> struct ComponentMatch<Separation2D>
    {
        static bool test(const Component *component)
        {
            return dynamic_cast<const Separation2D *>(component) != nullptr;
        }
    };

}
