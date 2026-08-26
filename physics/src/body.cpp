#include "kx/body.h"
#include "kx/internal/triangulate.h"

#include <cmath>

namespace kx
{

    Body::Body()
        : mType(BodyType::Static),
          mPosition(0.0f, 0.0f), mAngle(0.0f),
          mLinearVelocity(0.0f, 0.0f), mAngularVelocity(0.0f),
          mSleepTime(0.0f), mAwake(true), mFixedRotation(false), mBullet(false),
          mInvMass(0.0f), mInvI(0.0f), mLocalCenter(0.0f, 0.0f),
          mFriction(0.2f), mRestitution(0.0f),
          mLinearDamping(0.0f), mAngularDamping(0.0f), mGravityScale(1.0f),
          mForce(0.0f, 0.0f), mTorque(0.0f), mId(0),
          mUserData(nullptr), mContactCallback(nullptr), mContactContext(nullptr),
          mProxyId(-1), mProxyPosition(0.0f, 0.0f),
          mTightAABB{Math::Vec2(0.0f), Math::Vec2(0.0f)},
          mShapeCount(0)
    {
    }

    int Body::AddCircle(const Math::Vec2 &localCenter, float radius, float density)
    {
        if (mShapeCount >= kMaxShapes)
            return 0;

        Shape &shape = mShapes[mShapeCount++];
        shape.type = ShapeType::Circle;
        shape.density = density;
        shape.filter = mDefaultFilter;
        shape.isSensor = false;
        shape.userData = nullptr;
        shape.circle.center = localCenter;
        shape.circle.radius = radius;

        RecomputeMass();
        return 1;
    }

    int Body::AddBox(float halfWidth, float halfHeight, const Math::Vec2 &localCenter, float density)
    {
        if (mShapeCount >= kMaxShapes)
            return 0;

        Shape &shape = mShapes[mShapeCount++];
        shape.type = ShapeType::Polygon;
        shape.density = density;
        shape.filter = mDefaultFilter;
        shape.isSensor = false;
        shape.userData = nullptr;
        shape.polygon.SetAsBox(halfWidth, halfHeight, localCenter, 0.0f);

        RecomputeMass();
        return 1;
    }

    int Body::AddPolygon(const Math::Vec2 *points, int count, float density)
    {
        if (mShapeCount >= kMaxShapes || count < 3)
            return 0;

        Shape &shape = mShapes[mShapeCount++];
        shape.type = ShapeType::Polygon;
        shape.density = density;
        shape.filter = mDefaultFilter;
        shape.isSensor = false;
        shape.userData = nullptr;
        shape.polygon.Set(points, count);

        RecomputeMass();
        return 1;
    }

    int Body::AddEdge(const Math::Vec2 &localA, const Math::Vec2 &localB)
    {
        if (mShapeCount >= kMaxShapes)
            return 0;

        Shape &shape = mShapes[mShapeCount++];
        shape.type = ShapeType::Edge;
        shape.density = 0.0f;
        shape.filter = mDefaultFilter;
        shape.isSensor = false;
        shape.userData = nullptr;
        shape.edge.SetTwoSided(localA, localB);

        RecomputeMass();
        return 1;
    }

    int Body::AddChain(const Math::Vec2 *points, int count, bool loop)
    {
        int minCount = loop ? 3 : 2;
        if (count < minCount)
            return 0;

        int segments = loop ? count : count - 1;
        int added = 0;

        for (int i = 0; i < segments && mShapeCount < kMaxShapes; ++i)
        {
            int i1 = i;
            int i2 = (i + 1) % count;
            Math::Vec2 v1 = points[i1];
            Math::Vec2 v2 = points[i2];

            Math::Vec2 vPrev = v1;
            Math::Vec2 vNext = v2;
            if (loop || i > 0)
                vPrev = points[(i1 - 1 + count) % count];
            if (loop || i + 2 < count)
                vNext = points[(i2 + 1) % count];

            Shape &shape = mShapes[mShapeCount++];
            shape.type = ShapeType::Edge;
            shape.density = 0.0f;
            shape.filter = mDefaultFilter;
            shape.isSensor = false;
            shape.userData = nullptr;
            shape.edge.vertex1 = v1;
            shape.edge.vertex2 = v2;
            shape.edge.vertex0 = vPrev;
            shape.edge.vertex3 = vNext;
            shape.edge.radius = kPolygonRadius;
            shape.edge.oneSided = true;

            ++added;
        }

        if (added > 0)
            RecomputeMass();
        return added;
    }

    int Body::AddMesh(const Math::Vec2 *outline, int count, float density)
    {
        if (count < 3)
            return 0;

        Math::Vec2 triangles[kMaxShapes * 3];
        int triCount = Triangulate(outline, count, triangles, kMaxShapes);

        int added = 0;
        for (int i = 0; i < triCount; ++i)
            added += AddPolygon(triangles + i * 3, 3, density);

        return added;
    }

    void Body::RecomputeMass()
    {
        mInvMass = 0.0f;
        mInvI = 0.0f;
        mLocalCenter = Math::Vec2(0.0f, 0.0f);

        if (mType != BodyType::Dynamic)
            return;

        float mass = 0.0f;
        Math::Vec2 center(0.0f, 0.0f);
        float I = 0.0f;

        for (int i = 0; i < mShapeCount; ++i)
        {
            const Shape &shape = mShapes[i];
            MassData massData;
            switch (shape.type)
            {
            case ShapeType::Circle:
                massData = shape.circle.ComputeMass(shape.density);
                break;
            case ShapeType::Polygon:
                massData = shape.polygon.ComputeMass(shape.density);
                break;
            case ShapeType::Edge:
            default:
                massData = shape.edge.ComputeMass(shape.density);
                break;
            }

            mass += massData.mass;
            center += massData.mass * massData.center;
            I += massData.I;
        }

        if (mass > 0.0f)
        {
            mInvMass = 1.0f / mass;
            center *= mInvMass;
        }
        else
        {
            mass = 1.0f;
            mInvMass = 1.0f;
        }

        if (I > 0.0f)
        {
            I -= mass * Dot(center, center);
            mInvI = I > 0.0f ? 1.0f / I : 0.0f;
        }

        if (mFixedRotation)
            mInvI = 0.0f;

        mLocalCenter = center;
    }

    void Body::IntegrateVelocity(const Math::Vec2 &gravity, float dt)
    {
        if (mType != BodyType::Dynamic)
            return;

        if (!mAwake)
        {

            mForce = Math::Vec2(0.0f);
            mTorque = 0.0f;
            return;
        }

        mLinearVelocity += dt * (mGravityScale * gravity + mInvMass * mForce);
        mAngularVelocity += dt * mInvI * mTorque;

        mLinearVelocity *= 1.0f / (1.0f + dt * mLinearDamping);
        mAngularVelocity *= 1.0f / (1.0f + dt * mAngularDamping);

        mForce = Math::Vec2(0.0f);
        mTorque = 0.0f;
    }

    void Body::IntegratePosition(float dt)
    {
        if (mType == BodyType::Static || !mAwake)
            return;

        Math::Vec2 translation = dt * mLinearVelocity;
        float translationSq = translation.x * translation.x + translation.y * translation.y;
        if (translationSq > kMaxTranslation * kMaxTranslation)
        {
            float ratio = kMaxTranslation / sqrtf(translationSq);
            mLinearVelocity *= ratio;
            translation *= ratio;
        }

        Math::Vec2 center = GetTransform().Transform(mLocalCenter);
        center += translation;
        mAngle += dt * mAngularVelocity;
        Transform xf = MakeTransform(Math::Vec2(0.0f, 0.0f), mAngle);
        mPosition = center - xf.Transform(mLocalCenter);
    }

    void Body::ShiftCenter(const Math::Vec2 &deltaCenter, float deltaAngle)
    {
        Math::Vec2 center = GetTransform().Transform(mLocalCenter);
        center += deltaCenter;
        mAngle += deltaAngle;
        Transform xf = MakeTransform(Math::Vec2(0.0f, 0.0f), mAngle);
        mPosition = center - xf.Transform(mLocalCenter);
    }

} 