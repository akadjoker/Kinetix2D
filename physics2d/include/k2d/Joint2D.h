#pragma once

#include "k2d/Component.h"

#include <ct/string.hpp>

namespace k2d
{

    class Scene;
    class RigidBody2D;

    class Joint2D : public Component
    {
    public:
        static const ComponentType Type = ComponentType::Joint;

        Joint2D();
        ~Joint2D() override;

        void setTargetName(const char *name);
        const ct::String &targetName() const { return mTargetName; }

        bool collideConnected() const { return mCollideConnected; }
        void setCollideConnected(bool collide) { mCollideConnected = collide; }

        RigidBody2D *bodyA() const { return mBodyA; }
        RigidBody2D *bodyB() const { return mBodyB; }
        bool isConnected() const { return mBodyB != nullptr; }

        virtual Math::Vec2 anchorA() const = 0;
        virtual Math::Vec2 anchorB() const = 0;

    protected:
        // Target names are set one statement after addComponent, so the
        // lookup cannot happen on attach; the topology gate keeps it cheap.
        virtual void resolve();

        virtual void onConnected() {}

        virtual void initVelocity(float dt) = 0;
        virtual void solveVelocity(float dt) = 0;
        virtual bool solvePosition() { return true; }

        virtual bool dependsOnBody(const RigidBody2D *body) const
        {
            (void)body;
            return false;
        }
        virtual bool dependsOnJoint(const Joint2D *joint) const
        {
            (void)joint;
            return false;
        }

        virtual void invalidate() { mBodyA = mBodyB = nullptr; }

        RigidBody2D *mBodyA;
        RigidBody2D *mBodyB;
        uint32_t mResolvedVersion;

    private:
        friend class Scene;

        ct::String mTargetName;
        bool mCollideConnected;
    };

}
