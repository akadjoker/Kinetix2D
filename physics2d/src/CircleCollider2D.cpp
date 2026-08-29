#include "k2d/CircleCollider2D.h"
#include "k2d/RigidBody2D.h"

namespace k2d
{

    namespace
    {
        constexpr float kMinRadius = 0.01f;
    }

    CircleCollider2D::CircleCollider2D() : mRadius(16.0f)
    {
    }

    void CircleCollider2D::setRadius(float radius)
    {
        mRadius = radius > 0.0f ? radius : 0.0f;
        markDirty();
    }

    int CircleCollider2D::addTo(RigidBody2D &body, float density, float scaleX, float scaleY) const
    {
        const float scale = scaleX > scaleY ? scaleX : scaleY;
        const float radius = mRadius * scale;
        return body.AddCircle(mOffset, radius > kMinRadius ? radius : kMinRadius, density);
    }

}
