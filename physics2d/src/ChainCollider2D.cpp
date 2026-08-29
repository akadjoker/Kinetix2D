#include "k2d/ChainCollider2D.h"
#include "k2d/RigidBody2D.h"

namespace k2d
{

    ChainCollider2D::ChainCollider2D() : mLoop(false), mOneWay(false)
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

    void ChainCollider2D::setOneWay(bool oneWay)
    {
        mOneWay = oneWay;
        markDirty();
    }

    int ChainCollider2D::addTo(RigidBody2D &body, float, float scaleX, float scaleY) const
    {
        if (mPoints.size() < 2)
            return 0;

        ct::Vector<Math::Vec2> scaled;
        for (size_t i = 0; i < mPoints.size(); ++i)
            scaled.push_back(Math::Vec2(mPoints[i].x * scaleX + mOffset.x,
                                        mPoints[i].y * scaleY + mOffset.y));
        return body.AddChain(scaled.data(), (int)scaled.size(), mLoop, mOneWay);
    }

}
