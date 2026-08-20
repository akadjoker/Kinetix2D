#pragma once

#include <glm/glm.hpp>
#include <cstdint>

#include "common.h"
#include "shapes.h"

namespace kx
{

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

    struct Shape
    {
        ShapeType type;
        float density;
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
        static constexpr int kMaxShapes = 8;

        Body();

        Transform GetTransform() const { return MakeTransform(mPosition, mAngle); }

        const glm::vec2 &Position() const { return mPosition; }
        void SetPosition(const glm::vec2 &position) { mPosition = position; }

        const glm::vec2 &Velocity() const { return mLinearVelocity; }
        void SetVelocity(const glm::vec2 &v) { mLinearVelocity = v; }

        float AngularVelocity() const { return mAngularVelocity; }
        void SetAngularVelocity(float w) { mAngularVelocity = w; }

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
            mLinearVelocity += mInvMass * impulse;
            mAngularVelocity += mInvI * Cross(point - WorldCenter(), impulse);
        }

        BodyType Type() const { return mType; }

        const Shape *Shapes() const { return mShapes; }
        int ShapeCount() const { return mShapeCount; }

        void AddCircle(const glm::vec2 &localCenter, float radius, float density);
        void AddBox(float halfWidth, float halfHeight, float density, const glm::vec2 &localCenter = glm::vec2(0.0f, 0.0f));
        void AddEdge(const glm::vec2 &localA, const glm::vec2 &localB);

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

        float mInvMass;
        float mInvI;
        glm::vec2 mLocalCenter;
        float mFriction;
        float mRestitution;
        uint32_t mId;

        Shape mShapes[kMaxShapes];
        int mShapeCount;

        friend class World;
    };

} // namespace kx
