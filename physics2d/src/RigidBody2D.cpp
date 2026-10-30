#include "k2d/RigidBody2D.h"

#include "k2d/GameObject.h"
#include "k2d/Scene.h"
#include "k2d/Triangulate2D.h"
#include "ImageShape2D.h"

#include <cmath>

namespace k2d
{

namespace
{
constexpr float kDegToRad = 0.01745329251994329577f;
constexpr float kRadToDeg = 57.29577951308232088f;
constexpr int kMaxMeshTriangles = 32;
}

RigidBody2D::RigidBody2D()
    : Component(Type, ComponentEventNone), mBodyType(BodyType::Dynamic),
      mPosition(0.0f, 0.0f), mAngle(0.0f), mLinearVelocity(0.0f, 0.0f), mAngularVelocity(0.0f),
      mSleepTime(0.0f), mAwake(true), mFixedRotation(false), mBullet(false),
      mInvMass(0.0f), mInvI(0.0f), mLocalCenter(0.0f, 0.0f),
      mFriction(0.3f), mRestitution(0.0f), mLinearDamping(0.0f), mAngularDamping(0.0f), mGravityScale(1.0f),
      mForce(0.0f, 0.0f), mTorque(0.0f), mId(0),
      mProxyId(-1), mProxyPosition(0.0f, 0.0f), mTightAABB{Math::Vec2(0.0f), Math::Vec2(0.0f)},
      mDensity(1.0f), mNeedsRebuild(false), mBodyIndex(InvalidBodyIndex)
{
}

RigidBody2D::~RigidBody2D()
{
    if (GameObject* object = owner())
        if (Scene* scene = object->scene())
            scene->detachBody(*this);
}

void RigidBody2D::markDirty()
{
    GameObject* object = owner();
    Scene* scene = object ? object->scene() : nullptr;
    if (scene)
        scene->markBodyDirty(*this);
    else
        mNeedsRebuild = true;
}

void RigidBody2D::setBodyType(BodyType type)
{
    if (mBodyType == type)
        return;
    mBodyType = type;
    if (type != BodyType::Dynamic)
    {
        mLinearVelocity = Math::Vec2(0.0f);
        mAngularVelocity = 0.0f;
    }
    SetAwake(true);
    RecomputeMass();
}

void RigidBody2D::setDensity(float density)
{
    const float clamped = density > 0.0f ? density : 0.0f;
    if (mDensity == clamped)
        return;
    mDensity = clamped;
    markDirty();
}

void RigidBody2D::setFriction(float friction)
{
    mFriction = friction > 0.0f ? friction : 0.0f;
}

void RigidBody2D::setRestitution(float restitution)
{
    mRestitution = restitution > 0.0f ? restitution : 0.0f;
}

void RigidBody2D::setLinearDamping(float damping)
{
    mLinearDamping = damping > 0.0f ? damping : 0.0f;
}

void RigidBody2D::setAngularDamping(float damping)
{
    mAngularDamping = damping > 0.0f ? damping : 0.0f;
}

void RigidBody2D::setGravityScale(float scale)
{
    mGravityScale = scale;
}

void RigidBody2D::setFixedRotation(bool fixed)
{
    mFixedRotation = fixed;
    SetAwake(true);
    RecomputeMass();
}

void RigidBody2D::setVelocity(const Math::Vec2 &velocity)
{
    mLinearVelocity = velocity;
    SetAwake(true);
}

float RigidBody2D::angularVelocity() const
{
    return mAngularVelocity * kRadToDeg;
}

void RigidBody2D::setAngularVelocity(float degreesPerSecond)
{
    mAngularVelocity = degreesPerSecond * kDegToRad;
    SetAwake(true);
}

void RigidBody2D::applyForce(const Math::Vec2 &force)
{
    if (mBodyType != BodyType::Dynamic)
        return;
    SetAwake(true);
    if (!mAwake)
        return;
    mForce += force;
}

void RigidBody2D::applyImpulse(const Math::Vec2 &impulse)
{
    if (Dot(impulse, impulse) > 0.0f)
        SetAwake(true);
    mLinearVelocity += mInvMass * impulse;
}

void RigidBody2D::applyTorque(float torque)
{
    if (mBodyType != BodyType::Dynamic)
        return;
    SetAwake(true);
    if (!mAwake)
        return;
    mTorque += torque;
}

int RigidBody2D::AddCircle(const Math::Vec2 &localCenter, float radius, float density)
{
    Shape shape;
    shape.type = ShapeType::Circle;
    shape.density = density;
    shape.filter = mDefaultFilter;
    shape.isSensor = false;
    shape.userData = nullptr;
    shape.circle.center = localCenter;
    shape.circle.radius = radius;
    mShapes.push_back(shape);

    RecomputeMass();
    return 1;
}

int RigidBody2D::AddBox(float halfWidth, float halfHeight, const Math::Vec2 &localCenter, float density)
{
    Shape shape;
    shape.type = ShapeType::Polygon;
    shape.density = density;
    shape.filter = mDefaultFilter;
    shape.isSensor = false;
    shape.userData = nullptr;
    shape.polygon.SetAsBox(halfWidth, halfHeight, localCenter, 0.0f);
    mShapes.push_back(shape);

    RecomputeMass();
    return 1;
}

int RigidBody2D::AddPolygon(const Math::Vec2 *points, int count, float density)
{
    if (count < 3)
        return 0;

    Shape shape;
    shape.type = ShapeType::Polygon;
    shape.density = density;
    shape.filter = mDefaultFilter;
    shape.isSensor = false;
    shape.userData = nullptr;
    shape.polygon.Set(points, count);
    mShapes.push_back(shape);

    RecomputeMass();
    return 1;
}

int RigidBody2D::AddEdge(const Math::Vec2 &localA, const Math::Vec2 &localB)
{
    Shape shape;
    shape.type = ShapeType::Edge;
    shape.density = 0.0f;
    shape.filter = mDefaultFilter;
    shape.isSensor = false;
    shape.userData = nullptr;
    shape.edge.SetTwoSided(localA, localB);
    mShapes.push_back(shape);

    RecomputeMass();
    return 1;
}

int RigidBody2D::AddChain(const Math::Vec2 *points, int count, bool loop, bool oneSided)
{
    int minCount = loop ? 3 : 2;
    if (count < minCount)
        return 0;

    int segments = loop ? count : count - 1;
    int added = 0;

    for (int i = 0; i < segments; ++i)
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

        Shape shape;
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
        shape.edge.oneSided = oneSided;
        mShapes.push_back(shape);

        ++added;
    }

    if (added > 0)
        RecomputeMass();
    return added;
}

int RigidBody2D::AddMesh(const Math::Vec2 *outline, int count, float density)
{
    if (count < 3)
        return 0;

    Math::Vec2 triangles[kMaxMeshTriangles * 3];
    int triCount = Triangulate(outline, count, triangles, kMaxMeshTriangles);

    int added = 0;
    for (int i = 0; i < triCount; ++i)
        added += AddPolygon(triangles + i * 3, 3, density);

    return added;
}

int RigidBody2D::AddFromImage(const unsigned char *pixels, int width, int height, int bpp,
                              unsigned char threshold, float density, float scale, float simplifyDegrees)
{
    if (!pixels || width <= 0 || height <= 0 || (bpp != 4 && bpp != 1))
        return 0;

    ct::Vector<Polyline> rawLines;
    int lineCount = TraceImageOutlines(pixels, width, height, bpp, threshold, rawLines);
    if (lineCount == 0)
        return 0;

    float toleranceRadians = simplifyDegrees * (kPi / 180.0f);

    ct::Vector<Polyline> simplified;
    simplified.reserve(rawLines.size());
    for (size_t i = 0; i < rawLines.size(); ++i)
    {
        Polyline out;
        SimplifyOutline(rawLines[i], toleranceRadians, out);
        if (out.points.size() >= 3)
            simplified.push_back(out);
    }

    if (simplified.size() == 0)
        return 0;

    for (size_t i = 0; i < simplified.size(); ++i)
    {
        size_t best = i;
        double bestArea = OutlineArea2(simplified[i]);
        for (size_t j = i + 1; j < simplified.size(); ++j)
        {
            double area = OutlineArea2(simplified[j]);
            if (area > bestArea)
            {
                bestArea = area;
                best = j;
            }
        }
        if (best != i)
        {
            Polyline tmp = simplified[i];
            simplified[i] = simplified[best];
            simplified[best] = tmp;
        }
    }

    Math::Vec2 center((float)width * 0.5f, (float)height * 0.5f);
    int added = 0;

    for (size_t i = 0; i < simplified.size(); ++i)
    {
        const ct::Vector<Math::Vec2> &pts = simplified[i].points;
        int count = (int)pts.size();
        if (count < 3)
            continue;

        int loopCount = count;
        if (pts[0] == pts.back())
            --loopCount;
        if (loopCount < 3)
            continue;

        Math::Vec2 local[512];
        if (loopCount > 512)
            loopCount = 512;
        for (int k = 0; k < loopCount; ++k)
            local[k] = (pts[(size_t)k] - center) * scale;

        added += AddMesh(local, loopCount, density);
    }

    return added;
}

void RigidBody2D::RecomputeMass()
{
    mInvMass = 0.0f;
    mInvI = 0.0f;
    mLocalCenter = Math::Vec2(0.0f, 0.0f);

    if (mBodyType != BodyType::Dynamic)
        return;

    float mass = 0.0f;
    Math::Vec2 center(0.0f, 0.0f);
    float I = 0.0f;

    for (size_t i = 0; i < mShapes.size(); ++i)
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

void RigidBody2D::IntegrateVelocity(const Math::Vec2 &gravity, float dt)
{
    if (mBodyType != BodyType::Dynamic)
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

void RigidBody2D::IntegratePosition(float dt)
{
    if (mBodyType == BodyType::Static || !mAwake)
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

void RigidBody2D::ShiftCenter(const Math::Vec2 &deltaCenter, float deltaAngle)
{
    Math::Vec2 center = GetTransform().Transform(mLocalCenter);
    center += deltaCenter;
    mAngle += deltaAngle;
    Transform xf = MakeTransform(Math::Vec2(0.0f, 0.0f), mAngle);
    mPosition = center - xf.Transform(mLocalCenter);
}

}
