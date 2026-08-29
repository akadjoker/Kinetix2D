#include "k2d/Scene.h"

#include "Collide2D.h"
#include "k2d/GameObject.h"
#include "k2d/Joint2D.h"
#include "k2d/RigidBody2D.h"

#include <cmath>

namespace k2d
{

namespace
{

AABB ComputeShapeAABB(const Shape& shape, const Transform& xf)
{
    switch (shape.type)
    {
    case ShapeType::Circle:
        return shape.circle.ComputeAABB(xf);
    case ShapeType::Polygon:
        return shape.polygon.ComputeAABB(xf);
    case ShapeType::Edge:
    default:
        return shape.edge.ComputeAABB(xf);
    }
}

AABB ComputeBodyAABB(const RigidBody2D& body)
{
    Transform xf = body.GetTransform();
    AABB aabb = ComputeShapeAABB(body.Shapes()[0], xf);

    for (int i = 1; i < body.ShapeCount(); ++i)
    {
        AABB shapeAabb = ComputeShapeAABB(body.Shapes()[(size_t)i], xf);
        aabb.lowerBound = Min(aabb.lowerBound, shapeAabb.lowerBound);
        aabb.upperBound = Max(aabb.upperBound, shapeAabb.upperBound);
    }

    return aabb;
}

bool ContactHasStatic(const ContactInfo& c)
{
    return c.a->bodyType() == BodyType::Static || c.b->bodyType() == BodyType::Static;
}

bool BodyIsActive(const RigidBody2D* b)
{
    return b->bodyType() == BodyType::Kinematic || (b->bodyType() == BodyType::Dynamic && b->IsAwake());
}

bool ContactIsActive(const ContactInfo& c)
{
    return BodyIsActive(c.a) || BodyIsActive(c.b);
}

bool JointIsActive(const Joint2D* joint)
{
    const RigidBody2D* a = joint->bodyA();
    const RigidBody2D* b = joint->bodyB();
    return (a && BodyIsActive(a)) || (b && BodyIsActive(b));
}

bool CollideShapePair(Manifold& manifold, bool& flip, const Shape& sA, const Transform& xfA, const Shape& sB,
                      const Transform& xfB)
{
    flip = false;

    if (sA.type == ShapeType::Edge && sB.type == ShapeType::Edge)
        return false;

    if (sA.type == ShapeType::Circle && sB.type == ShapeType::Circle)
    {
        CollideCircles(&manifold, sA.circle, xfA, sB.circle, xfB);
        return true;
    }

    if (sA.type == ShapeType::Polygon && sB.type == ShapeType::Polygon)
    {
        CollidePolygons(&manifold, sA.polygon, xfA, sB.polygon, xfB);
        return true;
    }

    if (sA.type == ShapeType::Polygon && sB.type == ShapeType::Circle)
    {
        CollidePolygonAndCircle(&manifold, sA.polygon, xfA, sB.circle, xfB);
        return true;
    }
    if (sA.type == ShapeType::Circle && sB.type == ShapeType::Polygon)
    {
        flip = true;
        CollidePolygonAndCircle(&manifold, sB.polygon, xfB, sA.circle, xfA);
        return true;
    }

    if (sA.type == ShapeType::Edge && sB.type == ShapeType::Circle)
    {
        CollideEdgeAndCircle(&manifold, sA.edge, xfA, sB.circle, xfB);
        return true;
    }
    if (sA.type == ShapeType::Circle && sB.type == ShapeType::Edge)
    {
        flip = true;
        CollideEdgeAndCircle(&manifold, sB.edge, xfB, sA.circle, xfA);
        return true;
    }

    if (sA.type == ShapeType::Edge && sB.type == ShapeType::Polygon)
    {
        CollideEdgeAndPolygon(&manifold, sA.edge, xfA, sB.polygon, xfB);
        return true;
    }
    flip = true;
    CollideEdgeAndPolygon(&manifold, sB.edge, xfB, sA.polygon, xfA);
    return true;
}

uint64_t PairKey(const RigidBody2D* a, const RigidBody2D* b)
{
    uint32_t lo = a->Id() < b->Id() ? a->Id() : b->Id();
    uint32_t hi = a->Id() < b->Id() ? b->Id() : a->Id();
    return (static_cast<uint64_t>(lo) << 32) | hi;
}

struct PairQueryVisitor
{
    const DynamicTree* tree;
    int32_t queryProxyId;
    ct::Vector<int32_t>* hits;

    bool QueryCallback(int32_t proxyId)
    {
        if (proxyId == queryProxyId)
            return true;
        if (tree->WasMoved(proxyId) && proxyId > queryProxyId)
            return true;
        hits->push_back(proxyId);
        return true;
    }
};

constexpr uint64_t kBodyIdBits = 27;
constexpr uint64_t kBodyIdMask = (uint64_t(1) << kBodyIdBits) - 1;

uint64_t ContactKey(const ContactInfo& c)
{
    return (static_cast<uint64_t>(c.a->Id()) << 37) | (static_cast<uint64_t>(c.b->Id()) << 10) |
           (static_cast<uint64_t>(c.shapeIndexA) << 5) | static_cast<uint64_t>(c.shapeIndexB);
}

} // namespace

void Scene::destroyBody(RigidBody2D* body)
{
    for (size_t i = 0; i < mJoints.size(); ++i)
    {
        Joint2D* joint = mJoints[i];
        if (joint->bodyA() == body || joint->bodyB() == body || joint->dependsOnBody(body))
            joint->invalidate();
    }

    removeBodyContactEvents(body);

    for (size_t i = 0; i < mContacts.size(); ++i)
    {
        ContactInfo& contact = mContacts[i];
        if (contact.a == body && contact.b->bodyType() == BodyType::Dynamic)
            contact.b->SetAwake(true);
        if (contact.b == body && contact.a->bodyType() == BodyType::Dynamic)
            contact.a->SetAwake(true);
    }
    for (auto& entry : mPairs)
    {
        RigidBody2D* other = nullptr;
        if (entry.value.a == body)
            other = entry.value.b;
        else if (entry.value.b == body)
            other = entry.value.a;
        if (other && other->bodyType() == BodyType::Dynamic)
            other->SetAwake(true);
    }

    if (body->mProxyId != kNullNode)
    {
        for (size_t i = 0; i < mMoveBuffer.size(); ++i)
            if (mMoveBuffer[i] == body->mProxyId)
                mMoveBuffer[i] = kNullNode;
        mTree.DestroyProxy(body->mProxyId);
        body->mProxyId = kNullNode;
    }

    mKeyScratch.clear();
    for (auto& entry : mPairs)
        if (entry.value.a == body || entry.value.b == body)
            mKeyScratch.push_back(entry.key);
    for (size_t i = 0; i < mKeyScratch.size(); ++i)
        mPairs.erase(mKeyScratch[i]);

    mKeyScratch.clear();
    for (auto& entry : mPairContactStates)
    {
        uint32_t idA = static_cast<uint32_t>((entry.key >> 37) & kBodyIdMask);
        uint32_t idB = static_cast<uint32_t>((entry.key >> 10) & kBodyIdMask);
        if (idA == body->mId || idB == body->mId)
            mKeyScratch.push_back(entry.key);
    }
    for (size_t i = 0; i < mKeyScratch.size(); ++i)
        mPairContactStates.erase(mKeyScratch[i]);

    if (body->mBodyIndex < mBodies.size() && mBodies[body->mBodyIndex] == body)
    {
        RigidBody2D* moved = mBodies.back();
        mBodies[body->mBodyIndex] = moved;
        moved->mBodyIndex = body->mBodyIndex;
        mBodies.pop_back();
    }
    mFreeBodyIds.push_back(body->mId);
    body->mBodyIndex = RigidBody2D::InvalidBodyIndex;
}

void Scene::step(float dt)
{
    if (dt <= 0.0f)
        return;

    ++mStepStamp;

    double t0 = mClock ? mClock() : 0.0;

    for (size_t i = 0; i < mBodies.size(); ++i)
        mBodies[i]->IntegrateVelocity(mGravity, dt);

    double t1 = mClock ? mClock() : 0.0;

    updateContacts();
    updateContactEvents();

    double t2 = mClock ? mClock() : 0.0;

    initContactConstraints();
    warmStartContacts();

    mStepProfile.solveVelocityContacts = 0.0f;
    for (size_t i = 0; i < mJoints.size(); ++i)
        if (mJoints[i]->isConnected() && JointIsActive(mJoints[i]))
            mJoints[i]->initVelocity(dt);

    for (int it = 0; it < mVelocityIterations; ++it)
    {
        for (size_t i = 0; i < mJoints.size(); ++i)
            if (mJoints[i]->isConnected() && JointIsActive(mJoints[i]))
                mJoints[i]->solveVelocity(dt);
        solveContactVelocities();
    }

    storeContactImpulses();

    double t3 = mClock ? mClock() : 0.0;

    mBulletSweeps.clear();
    for (size_t i = 0; i < mBodies.size(); ++i)
    {
        RigidBody2D* b = mBodies[i];
        if (b->IsBullet() && b->bodyType() == BodyType::Dynamic && b->IsAwake())
            mBulletSweeps.push_back(BulletSweep{b, b->WorldCenter()});
    }

    for (size_t i = 0; i < mBodies.size(); ++i)
        mBodies[i]->IntegratePosition(dt);

    double t4 = mClock ? mClock() : 0.0;

    solveContactPositions();
    solveBulletSweeps();
    updateSleeping(dt);

    if (mClock)
    {
        double t5 = mClock();
        mStepProfile.integrate = (float)((t1 - t0 + t4 - t3) * 1000.0);
        mStepProfile.narrowphase = mNarrowMs;
        mStepProfile.broadphase = (float)((t2 - t1) * 1000.0) - mNarrowMs;
        mStepProfile.solveVelocity = (float)((t3 - t2) * 1000.0);
        mStepProfile.solveVelocityJoints = mStepProfile.solveVelocity - mStepProfile.solveVelocityContacts;
        mStepProfile.solvePosition = (float)((t5 - t4) * 1000.0);
    }
}

void Scene::solveBulletSweeps()
{
    for (size_t i = 0; i < mBulletSweeps.size(); ++i)
    {
        RigidBody2D* body = mBulletSweeps[i].body;
        Math::Vec2 prevCenter = mBulletSweeps[i].prevCenter;
        Math::Vec2 newCenter = body->WorldCenter();
        Math::Vec2 delta = newCenter - prevCenter;

        float distSq = Dot(delta, delta);
        if (distSq < kLinearSlop * kLinearSlop)
            continue;

        RayCastHit hit;
        if (!rayCastClosest(prevCenter, delta, hit, 0xFFFF, false, body))
            continue;

        if (hit.body->bodyType() == BodyType::Dynamic)
            continue;

        float endSide = Dot(newCenter - hit.point, hit.normal);
        if (endSide >= -kLinearSlop)
            continue;

        float dist = std::sqrt(distSq);
        float safeFraction = hit.fraction - kLinearSlop / dist;
        if (safeFraction < 0.0f)
            safeFraction = 0.0f;
        Math::Vec2 safeCenter = prevCenter + safeFraction * delta;

        body->ShiftCenter(safeCenter - newCenter, 0.0f);
    }
}

void Scene::updateSleeping(float dt)
{
    mTouchingActive.clear();
    for (size_t ci = 0; ci < mContacts.size(); ++ci)
    {
        const ContactInfo& c = mContacts[ci];
        if (c.sensor)
            continue;
        if (BodyIsActive(c.a) && c.b->bodyType() == BodyType::Dynamic)
            mTouchingActive.put(c.b, (unsigned char)1);
        if (BodyIsActive(c.b) && c.a->bodyType() == BodyType::Dynamic)
            mTouchingActive.put(c.a, (unsigned char)1);
    }

    float linearThresholdSquared = kSleepVelocity * kSleepVelocity;
    for (size_t i = 0; i < mBodies.size(); ++i)
    {
        RigidBody2D* body = mBodies[i];
        if (body->bodyType() != BodyType::Dynamic)
            continue;

        bool touchingActive = mTouchingActive.find(body) != nullptr;

        if (!body->mAwake)
        {
            if (touchingActive)
                body->SetAwake(true);
            else
                continue;
        }

        if (touchingActive)
        {
            body->mSleepTime = 0.0f;
            continue;
        }

        float linearSpeedSquared = Dot(body->mLinearVelocity, body->mLinearVelocity);
        if (linearSpeedSquared > linearThresholdSquared || std::fabs(body->mAngularVelocity) > kSleepAngularVelocity)
        {
            body->mSleepTime = 0.0f;
            continue;
        }

        body->mSleepTime += dt;
        if (body->mSleepTime >= kTimeToSleep)
            body->SetAwake(false);
    }
}

void Scene::solveContactPointPosition(ContactInfo& c, int pointIndex, float baumgarte)
{
    RigidBody2D* a = c.a;
    RigidBody2D* b = c.b;
    float radiusA = ShapeRadius(a->Shapes()[(size_t)c.shapeIndexA]);
    float radiusB = ShapeRadius(b->Shapes()[(size_t)c.shapeIndexB]);

    Transform xfA = a->GetTransform();
    Transform xfB = b->GetTransform();

    Math::Vec2 normal;
    Math::Vec2 point;
    float separation;

    if (c.manifold.type == Manifold::kCircles)
    {
        Math::Vec2 pA = xfA.Transform(c.manifold.localPoint);
        Math::Vec2 pB = xfB.Transform(c.manifold.points[0].localPoint);
        Math::Vec2 d = pB - pA;
        float len = sqrtf(Dot(d, d));
        normal = len > kEpsilon ? d / len : Math::Vec2(0.0f, 1.0f);
        point = 0.5f * (pA + pB);
        separation = len - radiusA - radiusB;
    }
    else if (c.manifold.type == Manifold::kFaceA)
    {
        normal = Rotate(xfA, c.manifold.localNormal);
        Math::Vec2 planePoint = xfA.Transform(c.manifold.localPoint);
        Math::Vec2 clipPoint = xfB.Transform(c.manifold.points[pointIndex].localPoint);
        separation = Dot(clipPoint - planePoint, normal) - radiusA - radiusB;
        point = clipPoint;
    }
    else
    {
        normal = Rotate(xfB, c.manifold.localNormal);
        Math::Vec2 planePoint = xfB.Transform(c.manifold.localPoint);
        Math::Vec2 clipPoint = xfA.Transform(c.manifold.points[pointIndex].localPoint);
        separation = Dot(clipPoint - planePoint, normal) - radiusA - radiusB;
        point = clipPoint;
        normal = -normal;
    }

    Math::Vec2 rA = point - a->WorldCenter();
    Math::Vec2 rB = point - b->WorldCenter();

    float C = baumgarte * (separation + kLinearSlop);
    if (C < -kMaxLinearCorrection)
        C = -kMaxLinearCorrection;
    if (C > 0.0f)
        C = 0.0f;

    float rnA = Cross(rA, normal);
    float rnB = Cross(rB, normal);
    float K = a->mInvMass + b->mInvMass + a->mInvI * rnA * rnA + b->mInvI * rnB * rnB;

    float impulse = K > 0.0f ? -C / K : 0.0f;
    Math::Vec2 P = impulse * normal;

    a->ShiftCenter(-a->mInvMass * P, -a->mInvI * Cross(rA, P));
    b->ShiftCenter(b->mInvMass * P, b->mInvI * Cross(rB, P));
}

void Scene::solveContactPositions()
{
    const int kPositionIterations = 3;

    for (int it = 0; it < kPositionIterations; ++it)
    {
        for (size_t i = 0; i < mJoints.size(); ++i)
            if (mJoints[i]->isConnected() && JointIsActive(mJoints[i]))
                mJoints[i]->solvePosition();

        for (size_t i = 0; i < mDynamicContacts.size(); ++i)
        {
            ContactInfo& c = *mDynamicContacts[i];
            for (int p = 0; p < c.manifold.pointCount; ++p)
                solveContactPointPosition(c, p, kBaumgarte);
        }
        for (size_t i = 0; i < mStaticContacts.size(); ++i)
        {
            ContactInfo& c = *mStaticContacts[i];
            for (int p = 0; p < c.manifold.pointCount; ++p)
                solveContactPointPosition(c, p, kBaumgarte);
        }
    }
}

void Scene::initContactConstraints()
{
    mDynamicContacts.clear();
    mStaticContacts.clear();
    for (size_t ci = 0; ci < mContacts.size(); ++ci)
    {
        ContactInfo& c = mContacts[ci];
        if (c.sensor)
            continue;
        if (!ContactIsActive(c))
            continue;
        if (ContactHasStatic(c))
            mStaticContacts.push_back(&c);
        else
            mDynamicContacts.push_back(&c);
        RigidBody2D* a = c.a;
        RigidBody2D* b = c.b;

        c.friction = sqrtf(a->mFriction * b->mFriction);
        c.restitution = a->mRestitution > b->mRestitution ? a->mRestitution : b->mRestitution;

        float radiusA = ShapeRadius(a->Shapes()[(size_t)c.shapeIndexA]);
        float radiusB = ShapeRadius(b->Shapes()[(size_t)c.shapeIndexB]);

        WorldManifold wm;
        wm.Initialize(&c.manifold, a->GetTransform(), radiusA, b->GetTransform(), radiusB);

        c.normal = wm.normal;
        c.tangent = Math::Vec2(-wm.normal.y, wm.normal.x);

        Math::Vec2 centerA = a->WorldCenter();
        Math::Vec2 centerB = b->WorldCenter();

        PairContactState* stored = mPairContactStates.find(ContactKey(c));

        for (int i = 0; i < c.manifold.pointCount; ++i)
        {
            ManifoldPoint& mp = c.manifold.points[i];

            mp.normalImpulse = 0.0f;
            mp.tangentImpulse = 0.0f;
            if (stored)
            {
                for (int k = 0; k < stored->impulseCount; ++k)
                {
                    if (stored->idKey[k] == mp.id.key)
                    {
                        mp.normalImpulse = stored->normalImpulse[k];
                        mp.tangentImpulse = stored->tangentImpulse[k];
                        break;
                    }
                }
            }

            c.rA[i] = wm.points[i] - centerA;
            c.rB[i] = wm.points[i] - centerB;

            float rnA = Cross(c.rA[i], c.normal);
            float rnB = Cross(c.rB[i], c.normal);
            float kNormal = a->mInvMass + b->mInvMass + a->mInvI * rnA * rnA + b->mInvI * rnB * rnB;
            c.normalMass[i] = kNormal > 0.0f ? 1.0f / kNormal : 0.0f;

            float rtA = Cross(c.rA[i], c.tangent);
            float rtB = Cross(c.rB[i], c.tangent);
            float kTangent = a->mInvMass + b->mInvMass + a->mInvI * rtA * rtA + b->mInvI * rtB * rtB;
            c.tangentMass[i] = kTangent > 0.0f ? 1.0f / kTangent : 0.0f;

            Math::Vec2 dv = b->mLinearVelocity + Cross(b->mAngularVelocity, c.rB[i]) - a->mLinearVelocity -
                            Cross(a->mAngularVelocity, c.rA[i]);
            float vn = Dot(dv, c.normal);

            float bias = 0.0f;
            if (vn < -kVelocityThreshold)
                bias = -c.restitution * vn;
            c.velocityBias[i] = bias;
        }
    }
}

void Scene::warmStartContacts()
{
    const auto warmStart = [](ContactInfo* contact)
    {
        ContactInfo& c = *contact;
        for (int i = 0; i < c.manifold.pointCount; ++i)
        {
            const ManifoldPoint& mp = c.manifold.points[i];
            Math::Vec2 impulse = mp.normalImpulse * c.normal + mp.tangentImpulse * c.tangent;
            c.a->mLinearVelocity -= c.a->mInvMass * impulse;
            c.a->mAngularVelocity -= c.a->mInvI * Cross(c.rA[i], impulse);
            c.b->mLinearVelocity += c.b->mInvMass * impulse;
            c.b->mAngularVelocity += c.b->mInvI * Cross(c.rB[i], impulse);
        }
    };

    for (size_t i = 0; i < mDynamicContacts.size(); ++i)
        warmStart(mDynamicContacts[i]);
    for (size_t i = 0; i < mStaticContacts.size(); ++i)
        warmStart(mStaticContacts[i]);
}

void Scene::solveContactVelocitiesOne(ContactInfo& c)
{
    RigidBody2D* a = c.a;
    RigidBody2D* b = c.b;

    for (int i = 0; i < c.manifold.pointCount; ++i)
    {
        ManifoldPoint& mp = c.manifold.points[i];

        Math::Vec2 dv = b->mLinearVelocity + Cross(b->mAngularVelocity, c.rB[i]) - a->mLinearVelocity -
                        Cross(a->mAngularVelocity, c.rA[i]);
        float vt = Dot(dv, c.tangent);
        float lambda = c.tangentMass[i] * (-vt);

        float maxFriction = c.friction * mp.normalImpulse;
        float newImpulse = mp.tangentImpulse + lambda;
        if (newImpulse < -maxFriction)
            newImpulse = -maxFriction;
        else if (newImpulse > maxFriction)
            newImpulse = maxFriction;
        lambda = newImpulse - mp.tangentImpulse;
        mp.tangentImpulse = newImpulse;

        Math::Vec2 impulse = lambda * c.tangent;
        a->mLinearVelocity -= a->mInvMass * impulse;
        a->mAngularVelocity -= a->mInvI * Cross(c.rA[i], impulse);
        b->mLinearVelocity += b->mInvMass * impulse;
        b->mAngularVelocity += b->mInvI * Cross(c.rB[i], impulse);
    }

    for (int i = 0; i < c.manifold.pointCount; ++i)
    {
        ManifoldPoint& mp = c.manifold.points[i];

        Math::Vec2 dv = b->mLinearVelocity + Cross(b->mAngularVelocity, c.rB[i]) - a->mLinearVelocity -
                        Cross(a->mAngularVelocity, c.rA[i]);
        float vn = Dot(dv, c.normal);
        float lambda = -c.normalMass[i] * (vn - c.velocityBias[i]);

        float newImpulse = mp.normalImpulse + lambda;
        if (newImpulse < 0.0f)
            newImpulse = 0.0f;
        lambda = newImpulse - mp.normalImpulse;
        mp.normalImpulse = newImpulse;

        Math::Vec2 impulse = lambda * c.normal;
        a->mLinearVelocity -= a->mInvMass * impulse;
        a->mAngularVelocity -= a->mInvI * Cross(c.rA[i], impulse);
        b->mLinearVelocity += b->mInvMass * impulse;
        b->mAngularVelocity += b->mInvI * Cross(c.rB[i], impulse);
    }
}

void Scene::solveContactVelocities()
{
    double start = mClock ? mClock() : 0.0;
    for (size_t i = 0; i < mDynamicContacts.size(); ++i)
        solveContactVelocitiesOne(*mDynamicContacts[i]);
    for (size_t i = 0; i < mStaticContacts.size(); ++i)
        solveContactVelocitiesOne(*mStaticContacts[i]);
    if (mClock)
        mStepProfile.solveVelocityContacts += (float)((mClock() - start) * 1000.0);
}

void Scene::storeContactImpulses()
{
    for (size_t ci = 0; ci < mContacts.size(); ++ci)
    {
        const ContactInfo& c = mContacts[ci];
        if (c.sensor)
            continue;

        uint64_t key = ContactKey(c);
        PairContactState* stored = mPairContactStates.find(key);
        PairContactState fresh;
        PairContactState& entry = stored ? *stored : fresh;

        entry.a = c.a;
        entry.b = c.b;
        entry.shapeIndexA = c.shapeIndexA;
        entry.shapeIndexB = c.shapeIndexB;
        entry.sensor = c.sensor;
        entry.impulseCount = c.manifold.pointCount;
        for (int i = 0; i < c.manifold.pointCount; ++i)
        {
            entry.idKey[i] = c.manifold.points[i].id.key;
            entry.normalImpulse[i] = c.manifold.points[i].normalImpulse;
            entry.tangentImpulse[i] = c.manifold.points[i].tangentImpulse;
        }
        entry.stamp = mStepStamp;

        if (!stored)
            mPairContactStates.put(key, entry);
    }

    mKeyScratch.clear();
    for (auto& entry : mPairContactStates)
        if (entry.value.stamp != mStepStamp)
            mKeyScratch.push_back(entry.key);
    for (size_t i = 0; i < mKeyScratch.size(); ++i)
        mPairContactStates.erase(mKeyScratch[i]);
}

void Scene::updateContactEvents()
{
    for (size_t i = 0; i < mContacts.size(); ++i)
    {
        const ContactInfo& contact = mContacts[i];
        uint64_t key = ContactKey(contact);
        PairContactState* existing = mPairContactStates.find(key);
        ContactPhase phase = existing ? ContactPhase::Persist : ContactPhase::Begin;

        PairContactState current;
        if (existing)
            current = *existing;
        current.a = contact.a;
        current.b = contact.b;
        current.shapeIndexA = contact.shapeIndexA;
        current.shapeIndexB = contact.shapeIndexB;
        current.manifold = contact.manifold;
        current.sensor = contact.sensor;
        current.stamp = mStepStamp;
        mPairContactStates.put(key, current);
        dispatchContactEvent(phase, current);
    }

    mKeyScratch.clear();
    for (auto& entry : mPairContactStates)
    {
        if (entry.value.stamp != mStepStamp)
        {
            dispatchContactEvent(ContactPhase::End, entry.value);
            mKeyScratch.push_back(entry.key);
        }
    }
    for (size_t i = 0; i < mKeyScratch.size(); ++i)
        mPairContactStates.erase(mKeyScratch[i]);
}

void Scene::dispatchContactEvent(ContactPhase phase, const PairContactState& state)
{
    if (!mCollisionCallback || phase == ContactPhase::Persist)
        return;

    // Both sides of a contact get a callback, each seeing itself as "self" -
    // matching the two independently-registered per-body callbacks the
    // solver dispatched to before bodies and contact routing merged.
    const auto dispatchOne = [&](RigidBody2D* self, RigidBody2D* other, int shapeIndexSelf, int shapeIndexOther)
    {
        CollisionInfo info;
        info.self = objectForBody(self);
        info.other = objectForBody(other);
        info.hit = true;
        info.sensor = state.sensor;
        info.began = phase == ContactPhase::Begin;
        if (state.manifold.pointCount > 0 && self && other)
        {
            const float radiusSelf = ShapeRadius(self->Shapes()[(size_t)shapeIndexSelf]);
            const float radiusOther = ShapeRadius(other->Shapes()[(size_t)shapeIndexOther]);
            WorldManifold worldManifold;
            worldManifold.Initialize(&state.manifold, self->GetTransform(), radiusSelf, other->GetTransform(),
                                     radiusOther);
            info.point = worldManifold.points[0];
            info.normal = worldManifold.normal;
        }

        if (info.self && info.other)
            mCollisionCallback(info, mCollisionCallbackUser);
    };

    dispatchOne(state.a, state.b, state.shapeIndexA, state.shapeIndexB);
    dispatchOne(state.b, state.a, state.shapeIndexB, state.shapeIndexA);
}

void Scene::removeBodyContactEvents(RigidBody2D* body)
{
    mKeyScratch.clear();
    for (auto& entry : mPairContactStates)
    {
        if (entry.value.a == body || entry.value.b == body)
        {
            dispatchContactEvent(ContactPhase::End, entry.value);
            mKeyScratch.push_back(entry.key);
        }
    }
    for (size_t i = 0; i < mKeyScratch.size(); ++i)
        mPairContactStates.erase(mKeyScratch[i]);
}

void Scene::syncProxies()
{
    for (size_t i = 0; i < mBodies.size(); ++i)
    {
        RigidBody2D* b = mBodies[i];
        if (b->ShapeCount() == 0)
            continue;

        AABB aabb = ComputeBodyAABB(*b);
        b->mTightAABB = aabb;

        if (b->mProxyId == kNullNode)
        {
            b->mProxyId = mTree.CreateProxy(aabb, b);
            b->mProxyPosition = b->mPosition;
            mMoveBuffer.push_back(b->mProxyId);
            continue;
        }

        Math::Vec2 displacement = b->mPosition - b->mProxyPosition;
        b->mProxyPosition = b->mPosition;
        if (mTree.MoveProxy(b->mProxyId, aabb, displacement))
            mMoveBuffer.push_back(b->mProxyId);
    }
}

void Scene::findNewPairs()
{
    for (size_t i = 0; i < mMoveBuffer.size(); ++i)
    {
        int32_t queryProxyId = mMoveBuffer[i];
        if (queryProxyId == kNullNode)
            continue;

        mPairScratch.clear();
        PairQueryVisitor visitor{&mTree, queryProxyId, &mPairScratch};
        mTree.Query(&visitor, mTree.GetFatAABB(queryProxyId));

        RigidBody2D* self = static_cast<RigidBody2D*>(mTree.GetUserData(queryProxyId));
        for (size_t h = 0; h < mPairScratch.size(); ++h)
        {
            RigidBody2D* other = static_cast<RigidBody2D*>(mTree.GetUserData(mPairScratch[h]));
            if (self->bodyType() != BodyType::Dynamic && other->bodyType() != BodyType::Dynamic)
                continue;
            uint64_t key = PairKey(self, other);
            if (!mPairs.find(key))
                mPairs.put(key, BodyPair{self, other});
        }
    }

    for (size_t i = 0; i < mMoveBuffer.size(); ++i)
        if (mMoveBuffer[i] != kNullNode)
            mTree.ClearMoved(mMoveBuffer[i]);
    mMoveBuffer.clear();
}

bool Scene::jointsAllowCollision(const RigidBody2D* a, const RigidBody2D* b) const
{
    for (size_t i = 0; i < mJoints.size(); ++i)
    {
        Joint2D* joint = mJoints[i];
        RigidBody2D* ja = joint->bodyA();
        RigidBody2D* jb = joint->bodyB();
        if (((ja == a && jb == b) || (ja == b && jb == a)) && !joint->collideConnected())
            return false;
    }
    return true;
}

void Scene::collidePair(RigidBody2D* first, RigidBody2D* second)
{
    if (first->mId > second->mId)
    {
        RigidBody2D* tmp = first;
        first = second;
        second = tmp;
    }

    if (!jointsAllowCollision(first, second))
        return;

    Transform xfi = first->GetTransform();
    Transform xfj = second->GetTransform();

    for (int si = 0; si < first->ShapeCount(); ++si)
    {
        AABB aabbA = ComputeShapeAABB(first->Shapes()[(size_t)si], xfi);
        for (int sj = 0; sj < second->ShapeCount(); ++sj)
        {
            if (!ShouldCollide(first->Shapes()[(size_t)si].filter, second->Shapes()[(size_t)sj].filter))
                continue;

            AABB aabbB = ComputeShapeAABB(second->Shapes()[(size_t)sj], xfj);
            if (!TestOverlap(aabbA, aabbB))
                continue;

            Manifold manifold;
            bool flip;
            if (!CollideShapePair(manifold, flip, first->Shapes()[(size_t)si], xfi, second->Shapes()[(size_t)sj], xfj))
                continue;

            if (manifold.pointCount == 0)
                continue;

            ContactInfo info;
            info.a = flip ? second : first;
            info.b = flip ? first : second;
            info.shapeIndexA = flip ? sj : si;
            info.shapeIndexB = flip ? si : sj;
            info.manifold = manifold;
            info.sensor = first->Shapes()[(size_t)si].isSensor || second->Shapes()[(size_t)sj].isSensor;
            mContacts.push_back(info);
        }
    }
}

void Scene::updateContacts()
{
    mContacts.clear();
    if (mUseTree)
        updateContactsTree();
    else
        updateContactsBrute();
}

void Scene::updateContactsTree()
{
    syncProxies();
    findNewPairs();

    double n0 = mClock ? mClock() : 0.0;

    mKeyScratch.clear();
    for (auto& entry : mPairs)
    {
        RigidBody2D* a = entry.value.a;
        RigidBody2D* b = entry.value.b;

        if (!TestOverlap(mTree.GetFatAABB(a->mProxyId), mTree.GetFatAABB(b->mProxyId)))
        {
            mKeyScratch.push_back(entry.key);
            continue;
        }

        if (!TestOverlap(a->TightAABB(), b->TightAABB()))
            continue;

        collidePair(a, b);
    }
    for (size_t i = 0; i < mKeyScratch.size(); ++i)
        mPairs.erase(mKeyScratch[i]);

    if (mClock)
        mNarrowMs = (float)((mClock() - n0) * 1000.0);
}

void Scene::updateContactsBrute()
{
    for (size_t i = 0; i < mBodies.size(); ++i)
    {
        RigidBody2D* bi = mBodies[i];
        for (size_t j = i + 1; j < mBodies.size(); ++j)
        {
            RigidBody2D* bj = mBodies[j];

            if (bi->bodyType() != BodyType::Dynamic && bj->bodyType() != BodyType::Dynamic)
                continue;

            AABB aabbA = ComputeBodyAABB(*bi);
            AABB aabbB = ComputeBodyAABB(*bj);
            if (!TestOverlap(aabbA, aabbB))
                continue;

            collidePair(bi, bj);
        }
    }
}

}
