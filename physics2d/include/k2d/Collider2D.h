#pragma once

#include "k2d/Component.h"

#include <kx/body.h>

#include <mathc.h>

namespace k2d
{

    struct ScenePhysics;

    class Collider2D : public Component
    {
    public:
        static const ComponentType Type = ComponentType::Collider;

        Collider2D();

        virtual int addTo(kx::Body &body, float density, float scaleX, float scaleY) const = 0;

        const Math::Vec2 &offset() const { return mOffset; }
        void setOffset(const Math::Vec2 &offset);

        bool isSensor() const { return mSensor; }
        void setSensor(bool sensor);

        uint16_t category() const { return mCategory; }
        uint16_t mask() const { return mMask; }
        void setFilter(uint16_t category, uint16_t mask);

        int shapeIndex() const { return mShapeIndex; }
        int shapeCount() const { return mShapeCount; }
        bool attached() const { return mShapeIndex >= 0; }

    protected:
        void markDirty();

        Math::Vec2 mOffset;

    private:
        friend struct ScenePhysics;

        bool mSensor;
        uint16_t mCategory;
        uint16_t mMask;
        int mShapeIndex;
        int mShapeCount;
        bool mDirty;
    };

}
