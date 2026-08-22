#pragma once

#include <glm/glm.hpp>
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

        const glm::vec2 &Position() const { return mPosition; }
        void SetPosition(const glm::vec2 &position) { mPosition = position; SetAwake(true); }

        float Angle() const { return mAngle; }
        void SetAngle(float angle) { mAngle = angle; SetAwake(true); }

        const glm::vec2 &Velocity() const { return mLinearVelocity; }
        void SetVelocity(const glm::vec2 &v) { mLinearVelocity = v; SetAwake(true); }

        float AngularVelocity() const { return mAngularVelocity; }
        void SetAngularVelocity(float w) { mAngularVelocity = w; SetAwake(true); }

        bool FixedRotation() const { return mFixedRotation; }
        void SetFixedRotation(bool fixed)
        {
            mFixedRotation = fixed;
            SetAwake(true);
            RecomputeMass();
        }

        bool IsAwake() const { return mAwake; }
        void SetAwake(bool awake)
        {
            mAwake = awake;
            if (awake)
                mSleepTime = 0.0f;
            else
            {
                mLinearVelocity = glm::vec2(0.0f);
                mAngularVelocity = 0.0f;
            }
        }

        float Friction() const { return mFriction; }
        void SetFriction(float friction) { mFriction = friction; }

        float Restitution() const { return mRestitution; }
        void SetRestitution(float restitution) { mRestitution = restitution; }

        float Mass() const { return mInvMass > 0.0f ? 1.0f / mInvMass : 0.0f; }
        float InvMass() const { return mInvMass; }
        float InvI() const { return mInvI; }

        glm::vec2 WorldCenter() const { return GetTransform().Transform(mLocalCenter); }

        void ApplyImpulse(const glm::vec2 &impulse, const glm::vec2 &point)
        {
            if (Dot(impulse, impulse) > 0.0f)
                SetAwake(true);
            mLinearVelocity += mInvMass * impulse;
            mAngularVelocity += mInvI * Cross(point - WorldCenter(), impulse);
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

        int AddCircle(const glm::vec2 &localCenter, float radius, float density);
        int AddBox(float halfWidth, float halfHeight, const glm::vec2 &localCenter, float density);
        int AddEdge(const glm::vec2 &localA, const glm::vec2 &localB);
        int AddPolygon(const glm::vec2 *points, int count, float density);
        int AddMesh(const glm::vec2 *outline, int count, float density);
        int AddFromImage(const unsigned char *pixels, int width, int height, int bpp,
                         unsigned char threshold, float density, float scale = 1.0f, float simplifyDegrees = 2.0f);

        void IntegrateVelocity(const glm::vec2 &gravity, float dt);
        void IntegratePosition(float dt);
        void ShiftCenter(const glm::vec2 &deltaCenter, float deltaAngle);

    private:
        void RecomputeMass();

        BodyType mType;

        glm::vec2 mPosition;
        float mAngle;

        glm::vec2 mLinearVelocity;
        float mAngularVelocity;
        float mSleepTime;
        bool mAwake;
        bool mFixedRotation;

        float mInvMass;
        float mInvI;
        glm::vec2 mLocalCenter;
        float mFriction;
        float mRestitution;
        uint32_t mId;
        void *mUserData;
        ContactCallback mContactCallback;
        void *mContactContext;

        int32_t mProxyId;
        glm::vec2 mProxyPosition;

        Filter mDefaultFilter;
        Shape mShapes[kMaxShapes];
        int mShapeCount;

        friend class World;
    };

} // namespace kx
