#include "k2d/EdgeCollider2D.h"
#include "k2d/RigidBody2D.h"

namespace k2d
{

    EdgeCollider2D::EdgeCollider2D() : mStart(-32.0f, 0.0f), mEnd(32.0f, 0.0f)
    {
    }

    void EdgeCollider2D::setPoints(const Math::Vec2 &start, const Math::Vec2 &end)
    {
        mStart = start;
        mEnd = end;
        markDirty();
    }

    int EdgeCollider2D::addTo(RigidBody2D &body, float, float scaleX, float scaleY) const
    {
        const Math::Vec2 a(mStart.x * scaleX + mOffset.x, mStart.y * scaleY + mOffset.y);
        const Math::Vec2 b(mEnd.x * scaleX + mOffset.x, mEnd.y * scaleY + mOffset.y);
        return body.AddEdge(a, b);
    }

}
