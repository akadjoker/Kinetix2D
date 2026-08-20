#include "kx/world.h"
#include "kx/collide.h"

namespace kx
{

    namespace
    {

        AABB ComputeShapeAABB(const Shape &shape, const Transform &xf)
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

        AABB ComputeBodyAABB(const Body &body)
        {
            Transform xf = body.GetTransform();
            AABB aabb = ComputeShapeAABB(body.Shapes()[0], xf);

            for (int i = 1; i < body.ShapeCount(); ++i)
            {
                AABB shapeAabb = ComputeShapeAABB(body.Shapes()[i], xf);
                aabb.lowerBound = glm::min(aabb.lowerBound, shapeAabb.lowerBound);
                aabb.upperBound = glm::max(aabb.upperBound, shapeAabb.upperBound);
            }

            return aabb;
        }

        bool TestOverlap(const AABB &a, const AABB &b)
        {
            glm::vec2 d1 = b.lowerBound - a.upperBound;
            glm::vec2 d2 = a.lowerBound - b.upperBound;

            if (d1.x > 0.0f || d1.y > 0.0f)
                return false;
            if (d2.x > 0.0f || d2.y > 0.0f)
                return false;
            return true;
        }

        bool CollideShapePair(Manifold &manifold, bool &flip,
                              const Shape &sA, const Transform &xfA,
                              const Shape &sB, const Transform &xfB)
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

    } // namespace

    World::World(const glm::vec2 &gravity)
        : mGravity(gravity), mStepStamp(0), mNextBodyId(1), mVelocityIterations(8)
    {
    }

    World::~World()
    {
        for (size_t i = 0; i < mJoints.size(); ++i)
            delete mJoints[i];
    }

    Body *World::CreateBody(BodyType type, const glm::vec2 &pos, float angle)
    {
        Body *body = mBodyPool.create();
        body->mType = type;
        body->mPosition = pos;
        body->mAngle = angle;
        body->mId = mNextBodyId++;
        mBodies.push_back(body);
        return body;
    }

    Body *World::CreateBox(const glm::vec2 &pos, float halfWidth, float halfHeight, float density)
    {
        Body *body = CreateBody(BodyType::Dynamic, pos);
        body->AddBox(halfWidth, halfHeight, density);
        return body;
    }

    Body *World::CreateCircle(const glm::vec2 &pos, float radius, float density)
    {
        Body *body = CreateBody(BodyType::Dynamic, pos);
        body->AddCircle(glm::vec2(0.0f, 0.0f), radius, density);
        return body;
    }

    Body *World::CreateStaticBox(const glm::vec2 &pos, float halfWidth, float halfHeight)
    {
        Body *body = CreateBody(BodyType::Static, pos);
        body->AddBox(halfWidth, halfHeight, 1.0f);
        return body;
    }

    Body *World::CreateKinematicBox(const glm::vec2 &pos, float halfWidth, float halfHeight)
    {
        Body *body = CreateBody(BodyType::Kinematic, pos);
        body->AddBox(halfWidth, halfHeight, 1.0f);
        return body;
    }

    Body *World::CreateEdge(const glm::vec2 &a, const glm::vec2 &b)
    {
        Body *body = CreateBody(BodyType::Static, glm::vec2(0.0f, 0.0f));
        body->AddEdge(a, b);
        return body;
    }

    void World::Destroy(Body *body)
    {
        for (size_t i = 0; i < mBodies.size(); ++i)
        {
            if (mBodies[i] == body)
            {
                mBodies[i] = mBodies.back();
                mBodies.pop_back();
                break;
            }
        }
        mBodyPool.destroy(body);
    }

    void World::AddJoint(Joint *joint)
    {
        mJoints.push_back(joint);
    }

    void World::DestroyJoint(Joint *joint)
    {
        for (size_t i = 0; i < mJoints.size(); ++i)
        {
            if (mJoints[i] == joint)
            {
                mJoints[i] = mJoints.back();
                mJoints.pop_back();
                break;
            }
        }
        delete joint;
    }

    static bool ShapeContainsPoint(const Shape &shape, const Transform &xf, const glm::vec2 &point)
    {
        if (shape.type == ShapeType::Circle)
        {
            glm::vec2 center = xf.Transform(shape.circle.center);
            glm::vec2 d = point - center;
            return Dot(d, d) <= shape.circle.radius * shape.circle.radius;
        }
        if (shape.type == ShapeType::Polygon)
        {
            glm::vec2 local = InvTransformPoint(xf, point);
            const Polygon &poly = shape.polygon;
            for (int32_t i = 0; i < poly.count; ++i)
            {
                if (Dot(poly.normals[i], local - poly.vertices[i]) > 0.0f)
                    return false;
            }
            return true;
        }
        return false;
    }

    Body *World::BodyAtPoint(const glm::vec2 &point) const
    {
        for (size_t i = 0; i < mBodies.size(); ++i)
        {
            Body *body = mBodies[i];
            if (body->Type() != BodyType::Dynamic)
                continue;
            Transform xf = body->GetTransform();
            for (int s = 0; s < body->ShapeCount(); ++s)
            {
                if (ShapeContainsPoint(body->Shapes()[s], xf, point))
                    return body;
            }
        }
        return nullptr;
    }

    void World::UpdateContacts()
    {
        mContacts.clear();

        for (size_t i = 0; i < mBodies.size(); ++i)
        {
            Body *bi = mBodies[i];
            for (size_t j = i + 1; j < mBodies.size(); ++j)
            {
                Body *bj = mBodies[j];

                if (bi->Type() != BodyType::Dynamic && bj->Type() != BodyType::Dynamic)
                    continue;

                AABB aabbA = ComputeBodyAABB(*bi);
                AABB aabbB = ComputeBodyAABB(*bj);
                if (!TestOverlap(aabbA, aabbB))
                    continue;

                Transform xfi = bi->GetTransform();
                Transform xfj = bj->GetTransform();

                for (int si = 0; si < bi->ShapeCount(); ++si)
                {
                    for (int sj = 0; sj < bj->ShapeCount(); ++sj)
                    {
                        Manifold manifold;
                        bool flip;
                        if (!CollideShapePair(manifold, flip, bi->Shapes()[si], xfi, bj->Shapes()[sj], xfj))
                            continue;

                        if (manifold.pointCount == 0)
                            continue;

                        ContactInfo info;
                        info.a = flip ? bj : bi;
                        info.b = flip ? bi : bj;
                        info.shapeIndexA = flip ? sj : si;
                        info.shapeIndexB = flip ? si : sj;
                        info.manifold = manifold;
                        mContacts.push_back(info);
                    }
                }
            }
        }
    }

    void World::Step(float dt)
    {
        if (dt <= 0.0f)
            return;

        ++mStepStamp;

        for (size_t i = 0; i < mBodies.size(); ++i)
            mBodies[i]->IntegrateVelocity(mGravity, dt);

        UpdateContacts();
        InitContactConstraints(dt);
        WarmStartContacts();

        for (size_t i = 0; i < mJoints.size(); ++i)
            mJoints[i]->InitVelocity(dt);

        for (int it = 0; it < mVelocityIterations; ++it)
        {
            for (size_t i = 0; i < mJoints.size(); ++i)
                mJoints[i]->SolveVelocity(dt);
            SolveContactVelocities();
        }

        StoreContactImpulses();

        for (size_t i = 0; i < mBodies.size(); ++i)
            mBodies[i]->IntegratePosition(dt);

        SolveContactPositions();
    }

    void World::SolveContactPositions()
    {
        const int kPositionIterations = 3;

        for (int it = 0; it < kPositionIterations; ++it)
        {
            for (size_t ci = 0; ci < mContacts.size(); ++ci)
            {
                ContactInfo &c = mContacts[ci];
                Body *a = c.a;
                Body *b = c.b;

                float radiusA = ShapeRadius(a->Shapes()[c.shapeIndexA]);
                float radiusB = ShapeRadius(b->Shapes()[c.shapeIndexB]);

                for (int i = 0; i < c.manifold.pointCount; ++i)
                {
                    Transform xfA = a->GetTransform();
                    Transform xfB = b->GetTransform();

                    glm::vec2 normal;
                    glm::vec2 point;
                    float separation;

                    if (c.manifold.type == Manifold::kCircles)
                    {
                        glm::vec2 pA = xfA.Transform(c.manifold.localPoint);
                        glm::vec2 pB = xfB.Transform(c.manifold.points[0].localPoint);
                        glm::vec2 d = pB - pA;
                        float len = sqrtf(Dot(d, d));
                        normal = len > kEpsilon ? d / len : glm::vec2(0.0f, 1.0f);
                        point = 0.5f * (pA + pB);
                        separation = len - radiusA - radiusB;
                    }
                    else if (c.manifold.type == Manifold::kFaceA)
                    {
                        normal = Rotate(xfA, c.manifold.localNormal);
                        glm::vec2 planePoint = xfA.Transform(c.manifold.localPoint);
                        glm::vec2 clipPoint = xfB.Transform(c.manifold.points[i].localPoint);
                        separation = Dot(clipPoint - planePoint, normal) - radiusA - radiusB;
                        point = clipPoint;
                    }
                    else
                    {
                        normal = Rotate(xfB, c.manifold.localNormal);
                        glm::vec2 planePoint = xfB.Transform(c.manifold.localPoint);
                        glm::vec2 clipPoint = xfA.Transform(c.manifold.points[i].localPoint);
                        separation = Dot(clipPoint - planePoint, normal) - radiusA - radiusB;
                        point = clipPoint;
                        normal = -normal;
                    }

                    glm::vec2 rA = point - a->WorldCenter();
                    glm::vec2 rB = point - b->WorldCenter();

                    float C = kBaumgarte * (separation + kLinearSlop);
                    if (C < -kMaxLinearCorrection)
                        C = -kMaxLinearCorrection;
                    if (C > 0.0f)
                        C = 0.0f;

                    float rnA = Cross(rA, normal);
                    float rnB = Cross(rB, normal);
                    float K = a->mInvMass + b->mInvMass + a->mInvI * rnA * rnA + b->mInvI * rnB * rnB;

                    float impulse = K > 0.0f ? -C / K : 0.0f;
                    glm::vec2 P = impulse * normal;

                    a->ShiftCenter(-a->mInvMass * P, -a->mInvI * Cross(rA, P));
                    b->ShiftCenter(b->mInvMass * P, b->mInvI * Cross(rB, P));
                }
            }
        }
    }

    void World::InitContactConstraints(float dt)
    {
        const float invDt = 1.0f / dt;

        for (size_t ci = 0; ci < mContacts.size(); ++ci)
        {
            ContactInfo &c = mContacts[ci];
            Body *a = c.a;
            Body *b = c.b;

            c.friction = sqrtf(a->mFriction * b->mFriction);
            c.restitution = a->mRestitution > b->mRestitution ? a->mRestitution : b->mRestitution;

            float radiusA = ShapeRadius(a->Shapes()[c.shapeIndexA]);
            float radiusB = ShapeRadius(b->Shapes()[c.shapeIndexB]);

            WorldManifold wm;
            wm.Initialize(&c.manifold, a->GetTransform(), radiusA, b->GetTransform(), radiusB);

            c.normal = wm.normal;
            c.tangent = glm::vec2(-wm.normal.y, wm.normal.x);

            glm::vec2 centerA = a->WorldCenter();
            glm::vec2 centerB = b->WorldCenter();

            StoredImpulses *stored = mImpulseMap.find(ContactKey(c));

            for (int i = 0; i < c.manifold.pointCount; ++i)
            {
                ManifoldPoint &mp = c.manifold.points[i];

                mp.normalImpulse = 0.0f;
                mp.tangentImpulse = 0.0f;
                if (stored)
                {
                    for (int k = 0; k < stored->count; ++k)
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

                glm::vec2 dv = b->mLinearVelocity + Cross(b->mAngularVelocity, c.rB[i]) -
                               a->mLinearVelocity - Cross(a->mAngularVelocity, c.rA[i]);
                float vn = Dot(dv, c.normal);

                float bias = 0.0f;
                if (vn < -kVelocityThreshold)
                    bias = -c.restitution * vn;
                c.velocityBias[i] = bias;
            }
        }
    }

    void World::WarmStartContacts()
    {
        for (size_t ci = 0; ci < mContacts.size(); ++ci)
        {
            ContactInfo &c = mContacts[ci];
            for (int i = 0; i < c.manifold.pointCount; ++i)
            {
                const ManifoldPoint &mp = c.manifold.points[i];
                glm::vec2 impulse = mp.normalImpulse * c.normal + mp.tangentImpulse * c.tangent;
                c.a->mLinearVelocity -= c.a->mInvMass * impulse;
                c.a->mAngularVelocity -= c.a->mInvI * Cross(c.rA[i], impulse);
                c.b->mLinearVelocity += c.b->mInvMass * impulse;
                c.b->mAngularVelocity += c.b->mInvI * Cross(c.rB[i], impulse);
            }
        }
    }

    void World::SolveContactVelocities()
    {
        for (size_t ci = 0; ci < mContacts.size(); ++ci)
        {
            ContactInfo &c = mContacts[ci];
            Body *a = c.a;
            Body *b = c.b;

            for (int i = 0; i < c.manifold.pointCount; ++i)
            {
                ManifoldPoint &mp = c.manifold.points[i];

                glm::vec2 dv = b->mLinearVelocity + Cross(b->mAngularVelocity, c.rB[i]) -
                               a->mLinearVelocity - Cross(a->mAngularVelocity, c.rA[i]);
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

                glm::vec2 impulse = lambda * c.tangent;
                a->mLinearVelocity -= a->mInvMass * impulse;
                a->mAngularVelocity -= a->mInvI * Cross(c.rA[i], impulse);
                b->mLinearVelocity += b->mInvMass * impulse;
                b->mAngularVelocity += b->mInvI * Cross(c.rB[i], impulse);
            }

            for (int i = 0; i < c.manifold.pointCount; ++i)
            {
                ManifoldPoint &mp = c.manifold.points[i];

                glm::vec2 dv = b->mLinearVelocity + Cross(b->mAngularVelocity, c.rB[i]) -
                               a->mLinearVelocity - Cross(a->mAngularVelocity, c.rA[i]);
                float vn = Dot(dv, c.normal);
                float lambda = -c.normalMass[i] * (vn - c.velocityBias[i]);

                float newImpulse = mp.normalImpulse + lambda;
                if (newImpulse < 0.0f)
                    newImpulse = 0.0f;
                lambda = newImpulse - mp.normalImpulse;
                mp.normalImpulse = newImpulse;

                glm::vec2 impulse = lambda * c.normal;
                a->mLinearVelocity -= a->mInvMass * impulse;
                a->mAngularVelocity -= a->mInvI * Cross(c.rA[i], impulse);
                b->mLinearVelocity += b->mInvMass * impulse;
                b->mAngularVelocity += b->mInvI * Cross(c.rB[i], impulse);
            }
        }
    }

    void World::StoreContactImpulses()
    {
        for (size_t ci = 0; ci < mContacts.size(); ++ci)
        {
            const ContactInfo &c = mContacts[ci];
            StoredImpulses stored;
            stored.count = c.manifold.pointCount;
            for (int i = 0; i < c.manifold.pointCount; ++i)
            {
                stored.idKey[i] = c.manifold.points[i].id.key;
                stored.normalImpulse[i] = c.manifold.points[i].normalImpulse;
                stored.tangentImpulse[i] = c.manifold.points[i].tangentImpulse;
            }
            stored.stamp = mStepStamp;
            mImpulseMap.put(ContactKey(c), stored);
        }

        mStaleKeys.clear();
        for (auto &entry : mImpulseMap)
        {
            if (entry.value.stamp != mStepStamp)
                mStaleKeys.push_back(entry.key);
        }
        for (size_t i = 0; i < mStaleKeys.size(); ++i)
            mImpulseMap.erase(mStaleKeys[i]);
    }

} // namespace kx
