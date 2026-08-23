#pragma once

#include "k2d/Component.h"

#include <mathc.h>

#include <ct/vector.hpp>

namespace k2d
{

    class PhysicsWorld2D;

    enum class ColliderShape : uint8_t
    {
        Box,
        Circle,
        Polygon
    };

    class Collider2D : public Component
    {
    public:
        static const ComponentType Type = ComponentType::Collider;

        Collider2D();

        ColliderShape shape() const { return mShape; }
        void setShape(ColliderShape shape);

        const Math::Vec2 &size() const { return mSize; }
        void setSize(const Math::Vec2 &size);
        float radius() const { return mRadius; }
        void setRadius(float radius);
        const Math::Vec2 &offset() const { return mOffset; }
        void setOffset(const Math::Vec2 &offset);

        const ct::Vector<Math::Vec2> &points() const { return mPoints; }
        void setPoints(const Math::Vec2 *points, int count);

        bool isSensor() const { return mSensor; }
        void setSensor(bool sensor);

        uint16_t category() const { return mCategory; }
        uint16_t mask() const { return mMask; }
        void setFilter(uint16_t category, uint16_t mask);

        int shapeIndex() const { return mShapeIndex; }
        bool attached() const { return mShapeIndex >= 0; }

    private:
        friend class PhysicsWorld2D;

        ColliderShape mShape;
        Math::Vec2 mSize;
        float mRadius;
        Math::Vec2 mOffset;
        ct::Vector<Math::Vec2> mPoints;
        bool mSensor;
        uint16_t mCategory;
        uint16_t mMask;
        int mShapeIndex;
        bool mDirty;
    };

}
