#pragma once

#include "k2d/Component.h"

#include <kx/body.h>

namespace k2d
{

    class PhysicsWorld2D;

    class RigidBody2D : public Component
    {
    public:
        static const ComponentType Type = ComponentType::RigidBody;

        RigidBody2D();

        kx::BodyType bodyType() const { return mBodyType; }
        void setBodyType(kx::BodyType type);

        float density() const { return mDensity; }
        void setDensity(float density);
        float friction() const { return mFriction; }
        void setFriction(float friction);
        float restitution() const { return mRestitution; }
        void setRestitution(float restitution);
        float linearDamping() const { return mLinearDamping; }
        void setLinearDamping(float damping);
        float angularDamping() const { return mAngularDamping; }
        void setAngularDamping(float damping);
        float gravityScale() const { return mGravityScale; }
        void setGravityScale(float scale);
        bool fixedRotation() const { return mFixedRotation; }
        void setFixedRotation(bool fixed);
        bool bullet() const { return mBullet; }
        void setBullet(bool bullet);

        Math::Vec2 velocity() const;
        void setVelocity(const Math::Vec2 &velocity);
        float angularVelocity() const;
        void setAngularVelocity(float degreesPerSecond);

        void applyForce(const Math::Vec2 &force);
        void applyImpulse(const Math::Vec2 &impulse);
        void applyTorque(float torque);
        void wake();

        kx::Body *body() const { return mBody; }
        bool inWorld() const { return mBody != nullptr; }

    private:
        friend class PhysicsWorld2D;

        kx::Body *mBody;
        PhysicsWorld2D *mWorld;
        kx::BodyType mBodyType;
        float mDensity;
        float mFriction;
        float mRestitution;
        float mLinearDamping;
        float mAngularDamping;
        float mGravityScale;
        bool mFixedRotation;
        bool mBullet;

        Math::Vec2 mPendingVelocity;
        float mPendingAngularVelocity;
        bool mHasPendingVelocity;
        bool mHasPendingAngularVelocity;
    };

}
