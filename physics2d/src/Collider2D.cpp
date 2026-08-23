#include "k2d/Collider2D.h"

namespace k2d
{

    Collider2D::Collider2D()
        : Component(Type, ComponentEventNone), mShape(ColliderShape::Box),
          mSize(32.0f, 32.0f), mRadius(16.0f), mOffset(0.0f, 0.0f), mPoints(), mSensor(false),
          mCategory(1), mMask(0xFFFF), mShapeIndex(-1), mDirty(true)
    {
    }

    void Collider2D::setShape(ColliderShape shape)
    {
        mShape = shape;
        mDirty = true;
    }

    void Collider2D::setSize(const Math::Vec2 &size)
    {
        mSize = Math::Vec2(size.x > 0.0f ? size.x : 0.0f, size.y > 0.0f ? size.y : 0.0f);
        mDirty = true;
    }

    void Collider2D::setRadius(float radius)
    {
        mRadius = radius > 0.0f ? radius : 0.0f;
        mDirty = true;
    }

    void Collider2D::setOffset(const Math::Vec2 &offset)
    {
        mOffset = offset;
        mDirty = true;
    }

    void Collider2D::setPoints(const Math::Vec2 *points, int count)
    {
        mPoints.clear();
        for (int i = 0; i < count; ++i)
            mPoints.push_back(points[i]);
        mDirty = true;
    }

    void Collider2D::setSensor(bool sensor)
    {
        mSensor = sensor;
        mDirty = true;
    }

    void Collider2D::setFilter(uint16_t category, uint16_t mask)
    {
        mCategory = category;
        mMask = mask;
        mDirty = true;
    }

}
