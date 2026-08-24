#pragma once

#include "k2d/Collider2D.h"

#include <ct/vector.hpp>

namespace k2d
{

    class PolygonCollider2D : public Collider2D
    {
    public:
        PolygonCollider2D();

        int addTo(kx::Body &body, float density, float scaleX, float scaleY) const override;

        const ct::Vector<Math::Vec2> &points() const { return mPoints; }
        void setPoints(const Math::Vec2 *points, int count);
        void setRegular(int sides, float radius);

    private:
        ct::Vector<Math::Vec2> mPoints;
    };

    template <> struct ComponentMatch<PolygonCollider2D>
    {
        static bool test(const Component *component)
        {
            return dynamic_cast<const PolygonCollider2D *>(component) != nullptr;
        }
    };

}
