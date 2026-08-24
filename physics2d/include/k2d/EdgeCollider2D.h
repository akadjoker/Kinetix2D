#pragma once

#include "k2d/Collider2D.h"

namespace k2d
{

    class EdgeCollider2D : public Collider2D
    {
    public:
        EdgeCollider2D();

        int addTo(kx::Body &body, float density, float scaleX, float scaleY) const override;

        const Math::Vec2 &start() const { return mStart; }
        const Math::Vec2 &end() const { return mEnd; }
        void setPoints(const Math::Vec2 &start, const Math::Vec2 &end);

    private:
        Math::Vec2 mStart;
        Math::Vec2 mEnd;
    };

    template <> struct ComponentMatch<EdgeCollider2D>
    {
        static bool test(const Component *component)
        {
            return dynamic_cast<const EdgeCollider2D *>(component) != nullptr;
        }
    };

}
