#pragma once

#include "k2d/Collider2D.h"

namespace k2d
{

    class BoxCollider2D : public Collider2D
    {
    public:
        BoxCollider2D();

        int addTo(RigidBody2D &body, float density, float scaleX, float scaleY) const override;

        const Math::Vec2 &size() const { return mSize; }
        void setSize(const Math::Vec2 &size);

    private:
        Math::Vec2 mSize;
    };

    template <> struct ComponentMatch<BoxCollider2D>
    {
        static bool test(const Component *component)
        {
            return dynamic_cast<const BoxCollider2D *>(component) != nullptr;
        }
    };

}
