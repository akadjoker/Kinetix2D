#include "k2d/PolygonCollider2D.h"
#include "k2d/RigidBody2D.h"

#include <cmath>

namespace k2d
{

    PolygonCollider2D::PolygonCollider2D()
    {
        setRegular(4, 16.0f);
    }

    void PolygonCollider2D::setPoints(const Math::Vec2 *points, int count)
    {
        mPoints.clear();
        for (int i = 0; i < count; ++i)
            mPoints.push_back(points[i]);
        markDirty();
    }

    void PolygonCollider2D::setRegular(int sides, float radius)
    {
        if (sides < 3)
            sides = 3;
        mPoints.clear();
        for (int i = 0; i < sides; ++i)
        {
            const float angle = 6.28318530717958647692f * (float)i / (float)sides;
            mPoints.push_back(Math::Vec2(std::cos(angle) * radius, std::sin(angle) * radius));
        }
        markDirty();
    }

    int PolygonCollider2D::addTo(RigidBody2D &body, float density, float scaleX, float scaleY) const
    {
        if (mPoints.size() < 3)
            return 0;

        ct::Vector<Math::Vec2> scaled;
        for (size_t i = 0; i < mPoints.size(); ++i)
            scaled.push_back(Math::Vec2(mPoints[i].x * scaleX + mOffset.x,
                                        mPoints[i].y * scaleY + mOffset.y));
        return body.AddPolygon(scaled.data(), (int)scaled.size(), density);
    }

}
