#include "k2d/BoxCollider2D.h"

namespace k2d
{

    namespace
    {
        constexpr float kMinExtent = 0.01f;
    }

    BoxCollider2D::BoxCollider2D() : mSize(32.0f, 32.0f)
    {
    }

    void BoxCollider2D::setSize(const Math::Vec2 &size)
    {
        mSize = Math::Vec2(size.x > 0.0f ? size.x : 0.0f, size.y > 0.0f ? size.y : 0.0f);
        markDirty();
    }

    int BoxCollider2D::addTo(kx::Body &body, float density, float scaleX, float scaleY) const
    {
        float halfWidth = mSize.x * 0.5f * scaleX;
        float halfHeight = mSize.y * 0.5f * scaleY;
        if (halfWidth < kMinExtent)
            halfWidth = kMinExtent;
        if (halfHeight < kMinExtent)
            halfHeight = kMinExtent;
        return body.AddBox(halfWidth, halfHeight, mOffset, density);
    }

}
