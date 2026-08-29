#pragma once

#include "k2d/Component.h"
#include "k2d/Geometry2D.h"

#include <ct/vector.hpp>

#include <cstddef>
#include <limits>

namespace k2d
{

    class Scene;

    class RigidBody2D : public Component
    {
    public:
        static const ComponentType Type = ComponentType::RigidBody;

        RigidBody2D();
        ~RigidBody2D() override;

        BodyType bodyType() const { return mBodyType; }
        void setBodyType(BodyType type);

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
        void setBullet(bool bullet) { mBullet = bullet; }

        const Math::Vec2 &velocity() const { return mLinearVelocity; }
        void setVelocity(const Math::Vec2 &velocity);
        float angularVelocity() const;
        void setAngularVelocity(float degreesPerSecond);

        void applyForce(const Math::Vec2 &force);
        void applyImpulse(const Math::Vec2 &impulse);
        void applyTorque(float torque);
        void wake() { SetAwake(true); }

        bool inWorld() const { return mBodyIndex != InvalidBodyIndex; }

        // --- Solver-facing surface, used by Scene and by colliders/joints. ---
        Transform GetTransform() const { return MakeTransform(mPosition, mAngle); }

        const Math::Vec2 &Position() const { return mPosition; }
        void SetPosition(const Math::Vec2 &position)
        {
            mPosition = position;
            SetAwake(true);
        }

        float Angle() const { return mAngle; }
        void SetAngle(float angle)
        {
            mAngle = angle;
            SetAwake(true);
        }

        bool IsAwake() const { return mAwake; }
        void SetAwake(bool awake)
        {
            mAwake = awake;
            if (awake)
                mSleepTime = 0.0f;
            else
            {
                mLinearVelocity = Math::Vec2(0.0f);
                mAngularVelocity = 0.0f;
            }
        }

        bool IsBullet() const { return mBullet; }

        // Raw radian angular velocity, used by the solver and by joints (the
        // public angularVelocity()/setAngularVelocity() pair above works in
        // degrees for scripts and the inspector).
        float AngularVelocityRadians() const { return mAngularVelocity; }
        void SetAngularVelocityRadians(float radiansPerSecond)
        {
            mAngularVelocity = radiansPerSecond;
            SetAwake(true);
        }

        float Mass() const { return mInvMass > 0.0f ? 1.0f / mInvMass : 0.0f; }
        float InvMass() const { return mInvMass; }
        float InvI() const { return mInvI; }

        Math::Vec2 WorldCenter() const { return GetTransform().Transform(mLocalCenter); }

        uint32_t Id() const { return mId; }

        bool SetSensor(int shapeIndex, bool sensor)
        {
            if (shapeIndex < 0 || (size_t)shapeIndex >= mShapes.size())
                return false;
            mShapes[(size_t)shapeIndex].isSensor = sensor;
            SetAwake(true);
            return true;
        }

        bool IsSensor(int shapeIndex) const
        {
            return shapeIndex >= 0 && (size_t)shapeIndex < mShapes.size() && mShapes[(size_t)shapeIndex].isSensor;
        }

        bool SetShapeUserData(int shapeIndex, void *userData)
        {
            if (shapeIndex < 0 || (size_t)shapeIndex >= mShapes.size())
                return false;
            mShapes[(size_t)shapeIndex].userData = userData;
            return true;
        }

        void *ShapeUserData(int shapeIndex) const
        {
            return shapeIndex >= 0 && (size_t)shapeIndex < mShapes.size() ? mShapes[(size_t)shapeIndex].userData
                                                                          : nullptr;
        }

        const ct::Vector<Shape> &Shapes() const { return mShapes; }
        int ShapeCount() const { return (int)mShapes.size(); }
        const AABB &TightAABB() const { return mTightAABB; }
        void ClearShapes()
        {
            mShapes.clear();
            RecomputeMass();
        }

        void SetFilter(uint16_t category, uint16_t mask, int16_t group = 0)
        {
            mDefaultFilter.category = category;
            mDefaultFilter.mask = mask;
            mDefaultFilter.group = group;
            for (size_t i = 0; i < mShapes.size(); ++i)
                mShapes[i].filter = mDefaultFilter;
        }

        bool SetShapeFilter(int shapeIndex, uint16_t category, uint16_t mask, int16_t group = 0)
        {
            if (shapeIndex < 0 || (size_t)shapeIndex >= mShapes.size())
                return false;
            mShapes[(size_t)shapeIndex].filter.category = category;
            mShapes[(size_t)shapeIndex].filter.mask = mask;
            mShapes[(size_t)shapeIndex].filter.group = group;
            SetAwake(true);
            return true;
        }

        Filter ShapeFilter(int shapeIndex) const
        {
            return shapeIndex >= 0 && (size_t)shapeIndex < mShapes.size() ? mShapes[(size_t)shapeIndex].filter
                                                                          : Filter();
        }

        int AddCircle(const Math::Vec2 &localCenter, float radius, float density);
        int AddBox(float halfWidth, float halfHeight, const Math::Vec2 &localCenter, float density);
        int AddEdge(const Math::Vec2 &localA, const Math::Vec2 &localB);
        int AddChain(const Math::Vec2 *points, int count, bool loop);
        int AddPolygon(const Math::Vec2 *points, int count, float density);
        int AddMesh(const Math::Vec2 *outline, int count, float density);
        int AddFromImage(const unsigned char *pixels, int width, int height, int bpp,
                         unsigned char threshold, float density, float scale = 1.0f, float simplifyDegrees = 2.0f);

        void IntegrateVelocity(const Math::Vec2 &gravity, float dt);
        void IntegratePosition(float dt);
        void ShiftCenter(const Math::Vec2 &deltaCenter, float deltaAngle);

        void RecomputeMass();

    private:
        friend class Scene;

        void markDirty();

        BodyType mBodyType;

        Math::Vec2 mPosition;
        float mAngle;

        Math::Vec2 mLinearVelocity;
        float mAngularVelocity;
        float mSleepTime;
        bool mAwake;
        bool mFixedRotation;
        bool mBullet;

        float mInvMass;
        float mInvI;
        Math::Vec2 mLocalCenter;
        float mFriction;
        float mRestitution;
        float mLinearDamping;
        float mAngularDamping;
        float mGravityScale;
        Math::Vec2 mForce;
        float mTorque;
        uint32_t mId;

        int32_t mProxyId;
        Math::Vec2 mProxyPosition;
        AABB mTightAABB;

        Filter mDefaultFilter;
        ct::Vector<Shape> mShapes;

        float mDensity;
        bool mNeedsRebuild;

        static constexpr std::size_t InvalidBodyIndex = (std::numeric_limits<std::size_t>::max)();
        std::size_t mBodyIndex;
    };

}
