#include "k2d/Collider2D.h"

#include "k2d/GameObject.h"
#include "k2d/PhysicsWorld2D.h"
#include "k2d/RigidBody2D.h"

namespace k2d
{

    void Collider2D::markDirty()
    {
        mDirty = true;
        GameObject *object = owner();
        if (!object)
            return;

        const size_t count = object->componentCount<RigidBody2D>();
        for (size_t i = 0; i < count; ++i)
        {
            RigidBody2D *rigidBody = object->getComponentAt<RigidBody2D>(i);
            if (rigidBody && rigidBody->mWorld)
                rigidBody->mWorld->markDirty(*rigidBody);
        }
    }

    Collider2D::Collider2D()
        : Component(Type, ComponentEventNone), mOffset(0.0f, 0.0f), mSensor(false), mCategory(1),
          mMask(0xFFFF), mShapeIndex(-1), mShapeCount(0), mDirty(true)
    {
    }

    void Collider2D::setOffset(const Math::Vec2 &offset)
    {
        mOffset = offset;
        markDirty();
    }

    void Collider2D::setSensor(bool sensor)
    {
        mSensor = sensor;
        markDirty();
    }

    void Collider2D::setFilter(uint16_t category, uint16_t mask)
    {
        mCategory = category;
        mMask = mask;
        markDirty();
    }

}
