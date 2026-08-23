#pragma once

#include <mathc.h>
#include <cstdint>

#include "common.h"
#include "shapes.h"

namespace kx
{

    struct ContactEvent;
    typedef void (*ContactCallback)(const ContactEvent &event, void *context);

    enum class BodyType : unsigned char
    {
        Static,
        Kinematic,
        Dynamic
    };

    enum class ShapeType : unsigned char
    {
        Circle,
        Polygon,
        Edge
    };

    struct Filter
    {
        uint16_t category;
        uint16_t mask;
        int16_t group;

        Filter() : category(1), mask(0xFFFF), group(0)
        {
        }
    };

    inline bool ShouldCollide(const Filter &a, const Filter &b)
    {
        if (a.group == b.group && a.group != 0)
            return a.group > 0;
        return (a.mask & b.category) != 0 && (b.mask & a.category) != 0;
    }

    struct Shape
    {
        ShapeType type;
        float density;
        Filter filter;
        bool isSensor;
        void *userData;
        Circle circle;
        Polygon polygon;
        Edge edge;
    };

    inline float ShapeRadius(const Shape &shape)
    {
        switch (shape.type)
        {
        case ShapeType::Circle:
            return shape.circle.radius;
        case ShapeType::Polygon:
            return shape.polygon.radius;
        default:
            return shape.edge.radius;
        }
    }

    class Body
    {
    public:
        static constexpr int kMaxShapes = 32;

        Body();

        Transform GetTransform() const { return MakeTransform(mPosition, mAngle); }

        const Math::Vec2 &Position() const { return mPosition; }
        void SetPosition(const Math::Vec2 &position) { mPosition = position; SetAwake(true); }

        float Angle() const { return mAngle; }
        void SetAngle(float angle) { mAngle = angle; SetAwake(true); }

        const Math::Vec2 &Velocity() const { return mLinearVelocity; }
        void SetVelocity(const Math::Vec2 &v) { mLinearVelocity = v; SetAwake(true); }

        float AngularVelocity() const { return mAngularVelocity; }
        void SetAngularVelocity(float w) { mAngularVelocity = w; SetAwake(true); }

        bool FixedRotation() const { return mFixedRotation; }
        void SetFixedRotation(bool fixed)
        {
            mFixedRotation = fixed;
            SetAwake(true);
            RecomputeMass();
        }

        bool IsBullet() const { return mBullet; }
        void SetBullet(bool bullet) { mBullet = bullet; }

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

        float Friction() const { return mFriction; }
        void SetFriction(float friction) { mFriction = friction; }

        float Restitution() const { return mRestitution; }
        void SetRestitution(float restitution) { mRestitution = restitution; }

        float LinearDamping() const { return mLinearDamping; }
        void SetLinearDamping(float damping) { mLinearDamping = damping > 0.0f ? damping : 0.0f; }

        float AngularDamping() const { return mAngularDamping; }
        void SetAngularDamping(float damping) { mAngularDamping = damping > 0.0f ? damping : 0.0f; }

        float GravityScale() const { return mGravityScale; }
        void SetGravityScale(float scale) { mGravityScale = scale; }

        float Mass() const { return mInvMass > 0.0f ? 1.0f / mInvMass : 0.0f; }
        float InvMass() const { return mInvMass; }
        float InvI() const { return mInvI; }

        Math::Vec2 WorldCenter() const { return GetTransform().Transform(mLocalCenter); }

        void ApplyImpulse(const Math::Vec2 &impulse, const Math::Vec2 &point)
        {
            if (Dot(impulse, impulse) > 0.0f)
                SetAwake(true);
            mLinearVelocity += mInvMass * impulse;
            mAngularVelocity += mInvI * Cross(point - WorldCenter(), impulse);
        }

        void ApplyLinearImpulseToCenter(const Math::Vec2 &impulse, bool wake = true)
        {
            if (wake && Dot(impulse, impulse) > 0.0f)
                SetAwake(true);
            mLinearVelocity += mInvMass * impulse;
        }

        void ApplyAngularImpulse(float impulse, bool wake = true)
        {
            if (wake && impulse != 0.0f)
                SetAwake(true);
            mAngularVelocity += mInvI * impulse;
        }

        void ApplyForce(const Math::Vec2 &force, const Math::Vec2 &point, bool wake = true)
        {
            if (mType != BodyType::Dynamic)
                return;
            if (wake)
                SetAwake(true);
            if (!mAwake)
                return;
            mForce += force;
            mTorque += Cross(point - WorldCenter(), force);
        }

        void ApplyForceToCenter(const Math::Vec2 &force, bool wake = true)
        {
            if (mType != BodyType::Dynamic)
                return;
            if (wake)
                SetAwake(true);
            if (!mAwake)
                return;
            mForce += force;
        }

        void ApplyTorque(float torque, bool wake = true)
        {
            if (mType != BodyType::Dynamic)
                return;
            if (wake)
                SetAwake(true);
            if (!mAwake)
                return;
            mTorque += torque;
        }

        BodyType Type() const { return mType; }
        uint32_t Id() const { return mId; }

        void SetUserData(void *userData) { mUserData = userData; }
        void *UserData() const { return mUserData; }

        void SetContactCallback(ContactCallback callback, void *context = nullptr)
        {
            mContactCallback = callback;
            mContactContext = context;
        }

        bool SetSensor(int shapeIndex, bool sensor)
        {
            if (shapeIndex < 0 || shapeIndex >= mShapeCount)
                return false;
            mShapes[shapeIndex].isSensor = sensor;
            SetAwake(true);
            return true;
        }

        bool IsSensor(int shapeIndex) const
        {
            return shapeIndex >= 0 && shapeIndex < mShapeCount && mShapes[shapeIndex].isSensor;
        }

        bool SetShapeUserData(int shapeIndex, void *userData)
        {
            if (shapeIndex < 0 || shapeIndex >= mShapeCount)
                return false;
            mShapes[shapeIndex].userData = userData;
            return true;
        }

        void *ShapeUserData(int shapeIndex) const
        {
            return shapeIndex >= 0 && shapeIndex < mShapeCount ? mShapes[shapeIndex].userData : nullptr;
        }

        const Shape *Shapes() const { return mShapes; }
        int ShapeCount() const { return mShapeCount; }

        void SetFilter(uint16_t category, uint16_t mask, int16_t group = 0)
        {
            mDefaultFilter.category = category;
            mDefaultFilter.mask = mask;
            mDefaultFilter.group = group;
            for (int i = 0; i < mShapeCount; ++i)
                mShapes[i].filter = mDefaultFilter;
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

    private:
        void RecomputeMass();

        BodyType mType;

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
        void *mUserData;
        ContactCallback mContactCallback;
        void *mContactContext;

        int32_t mProxyId;
        Math::Vec2 mProxyPosition;

        Filter mDefaultFilter;
        Shape mShapes[kMaxShapes];
        int mShapeCount;

        friend class World;
    };

} 