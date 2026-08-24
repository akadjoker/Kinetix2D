#include "k2d/ChainCollider2D.h"

namespace k2d
{

    ChainCollider2D::ChainCollider2D() : mLoop(false)
    {
    }

    void ChainCollider2D::setPoints(const Math::Vec2 *points, int count)
    {
        mPoints.clear();
        for (int i = 0; i < count; ++i)
            mPoints.push_back(points[i]);
        markDirty();
    }

    void ChainCollider2D::setLoop(bool loop)
    {
        mLoop = loop;
        markDirty();
    }

    int ChainCollider2D::addTo(kx::Body &body, float, float scaleX, float scaleY) const
    {
        if (mPoints.size() < 2)
            return 0;

        ct::Vector<Math::Vec2> scaled;
        for (size_t i = 0; i < mPoints.size(); ++i)
            scaled.push_back(Math::Vec2(mPoints[i].x * scaleX + mOffset.x,
                                        mPoints[i].y * scaleY + mOffset.y));
        return body.AddChain(scaled.data(), (int)scaled.size(), mLoop);
    }

}
