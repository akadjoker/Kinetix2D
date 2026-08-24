#pragma once

#include "k2d/Collider2D.h"

namespace k2d
{

    class CircleCollider2D : public Collider2D
    {
    public:
        CircleCollider2D();

        int addTo(kx::Body &body, float density, float scaleX, float scaleY) const override;

        float radius() const { return mRadius; }
        void setRadius(float radius);

    private:
        float mRadius;
    };

    template <> struct ComponentMatch<CircleCollider2D>
    {
        static bool test(const Component *component)
        {
            return dynamic_cast<const CircleCollider2D *>(component) != nullptr;
        }
    };

}
