#include "kx/body.h"
#include "kx/internal/triangulate.h"

#include <cmath>

namespace kx
{

    Body::Body()
        : mType(BodyType::Static),
          mPosition(0.0f, 0.0f), mAngle(0.0f),
          mLinearVelocity(0.0f, 0.0f), mAngularVelocity(0.0f),
          mSleepTime(0.0f), mAwake(true),
          mInvMass(0.0f), mInvI(0.0f), mLocalCenter(0.0f, 0.0f),
          mFriction(0.2f), mRestitution(0.0f), mId(0),
          mProxyId(-1), mProxyPosition(0.0f, 0.0f),
          mShapeCount(0)
    {
    }

    int Body::AddCircle(const glm::vec2 &localCenter, float radius, float density)
    {
        if (mShapeCount >= kMaxShapes)
            return 0;

        Shape &shape = mShapes[mShapeCount++];
        shape.type = ShapeType::Circle;
        shape.density = density;
        shape.filter = mDefaultFilter;
        shape.circle.center = localCenter;
        shape.circle.radius = radius;

        RecomputeMass();
        return 1;
    }

    int Body::AddBox(float halfWidth, float halfHeight, const glm::vec2 &localCenter, float density)
    {
        if (mShapeCount >= kMaxShapes)
            return 0;

        Shape &shape = mShapes[mShapeCount++];
        shape.type = ShapeType::Polygon;
        shape.density = density;
        shape.filter = mDefaultFilter;
        shape.polygon.SetAsBox(halfWidth, halfHeight, localCenter, 0.0f);

        RecomputeMass();
        return 1;
    }

    int Body::AddPolygon(const glm::vec2 *points, int count, float density)
    {
        if (mShapeCount >= kMaxShapes || count < 3)
            return 0;

        Shape &shape = mShapes[mShapeCount++];
        shape.type = ShapeType::Polygon;
        shape.density = density;
        shape.filter = mDefaultFilter;
        shape.polygon.Set(points, count);

        RecomputeMass();
        return 1;
    }

    int Body::AddEdge(const glm::vec2 &localA, const glm::vec2 &localB)
    {
        if (mShapeCount >= kMaxShapes)
            return 0;

        Shape &shape = mShapes[mShapeCount++];
        shape.type = ShapeType::Edge;
        shape.density = 0.0f;
        shape.filter = mDefaultFilter;
        shape.edge.SetTwoSided(localA, localB);

        RecomputeMass();
        return 1;
    }

    int Body::AddMesh(const glm::vec2 *outline, int count, float density)
    {
        if (count < 3)
            return 0;

        glm::vec2 triangles[kMaxShapes * 3];
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
        mLocalCenter = glm::vec2(0.0f, 0.0f);

        if (mType != BodyType::Dynamic || !mAwake)
            return;

        float mass = 0.0f;
        glm::vec2 center(0.0f, 0.0f);
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

        mLocalCenter = center;
    }

    void Body::IntegrateVelocity(const glm::vec2 &gravity, float dt)
    {
        if (mType != BodyType::Dynamic)
            return;

        mLinearVelocity += dt * gravity;
    }

    void Body::IntegratePosition(float dt)
    {
        if (mType == BodyType::Static || !mAwake)
            return;

        glm::vec2 translation = dt * mLinearVelocity;
        float translationSq = translation.x * translation.x + translation.y * translation.y;
        if (translationSq > kMaxTranslation * kMaxTranslation)
        {
            float ratio = kMaxTranslation / sqrtf(translationSq);
            mLinearVelocity *= ratio;
            translation *= ratio;
        }

        glm::vec2 center = GetTransform().Transform(mLocalCenter);
        center += translation;
        mAngle += dt * mAngularVelocity;
        Transform xf = MakeTransform(glm::vec2(0.0f, 0.0f), mAngle);
        mPosition = center - xf.Transform(mLocalCenter);
    }

    void Body::ShiftCenter(const glm::vec2 &deltaCenter, float deltaAngle)
    {
        glm::vec2 center = GetTransform().Transform(mLocalCenter);
        center += deltaCenter;
        mAngle += deltaAngle;
        Transform xf = MakeTransform(glm::vec2(0.0f, 0.0f), mAngle);
        mPosition = center - xf.Transform(mLocalCenter);
    }

} // namespace kx
