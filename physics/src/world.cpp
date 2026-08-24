#include "kx/world.h"
#include "kx/internal/collide.h"
#include "kx/internal/raycast.h"

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
                aabb.lowerBound = Min(aabb.lowerBound, shapeAabb.lowerBound);
                aabb.upperBound = Max(aabb.upperBound, shapeAabb.upperBound);
            }

            return aabb;
        }

        bool ContactHasStatic(const ContactInfo &c)
        {
            return c.a->Type() == BodyType::Static || c.b->Type() == BodyType::Static;
        }

        bool BodyIsActive(const Body *b)
        {
            return b->Type() == BodyType::Kinematic ||
                   (b->Type() == BodyType::Dynamic && b->IsAwake());
        }

        bool ContactIsActive(const ContactInfo &c)
        {
            return BodyIsActive(c.a) || BodyIsActive(c.b);
        }

        bool JointIsActive(const Joint *joint)
        {
            const Body *a = joint->BodyA();
            const Body *b = joint->BodyB();
            return (a && BodyIsActive(a)) || (b && BodyIsActive(b));
        }

        struct BodyQueryVisitor
        {
            const DynamicTree *tree;
            ct::Vector<Body *> *hits;

            bool QueryCallback(int32_t proxyId)
            {
                hits->push_back(static_cast<Body *>(tree->GetUserData(proxyId)));
                return true;
            }
        };

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

    } 

    namespace
    {
        uint64_t PairKey(const Body *a, const Body *b);
    }

    World::World(const Math::Vec2 &gravity)
        : mGravity(gravity), mUseTree(true), mClock(nullptr),
          mStepStamp(0), mNextBodyId(1), mVelocityIterations(8)
    {
        mProfile = StepProfile{0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        mNarrowMs = 0.0f;
    }

    World::~World()
    {
        for (size_t i = 0; i < mJoints.size(); ++i)
            delete mJoints[i];
    }

    Body *World::CreateBody(BodyType type, const Math::Vec2 &pos, float angle)
    {
        Body *body = mBodyPool.create();
        body->mType = type;
        body->mPosition = pos;
        body->mAngle = angle;

        if (!mFreeBodyIds.empty())
        {
            body->mId = mFreeBodyIds.back();
            mFreeBodyIds.pop_back();
        }
        else
        {
            body->mId = mNextBodyId++;
        }
        mBodies.push_back(body);
        return body;
    }

    Body *World::CreateBox(const Math::Vec2 &pos, float halfWidth, float halfHeight, float density)
    {
        Body *body = CreateBody(BodyType::Dynamic, pos);
        body->AddBox(halfWidth, halfHeight, Math::Vec2(0.0f, 0.0f), density);
        return body;
    }

    Body *World::CreateCircle(const Math::Vec2 &pos, float radius, float density)
    {
        Body *body = CreateBody(BodyType::Dynamic, pos);
        body->AddCircle(Math::Vec2(0.0f, 0.0f), radius, density);
        return body;
    }

    Body *World::CreateStaticBox(const Math::Vec2 &pos, float halfWidth, float halfHeight)
    {
        Body *body = CreateBody(BodyType::Static, pos);
        body->AddBox(halfWidth, halfHeight, Math::Vec2(0.0f, 0.0f), 1.0f);
        return body;
    }

    Body *World::CreateKinematicBox(const Math::Vec2 &pos, float halfWidth, float halfHeight)
    {
        Body *body = CreateBody(BodyType::Kinematic, pos);
        body->AddBox(halfWidth, halfHeight, Math::Vec2(0.0f, 0.0f), 1.0f);
        return body;
    }

    Body *World::CreateEdge(const Math::Vec2 &a, const Math::Vec2 &b)
    {
        Body *body = CreateBody(BodyType::Static, Math::Vec2(0.0f, 0.0f));
        body->AddEdge(a, b);
        return body;
    }

    Body *World::CreateChain(const Math::Vec2 *points, int count, bool loop)
    {
        Body *body = CreateBody(BodyType::Static, Math::Vec2(0.0f, 0.0f));
        body->AddChain(points, count, loop);
        return body;
    }

    Body *World::CreatePolygon(const Math::Vec2 &pos, const Math::Vec2 *points, int count, float density)
    {
        Body *body = CreateBody(BodyType::Dynamic, pos);
        body->AddPolygon(points, count, density);
        return body;
    }

    Body *World::CreateMesh(const Math::Vec2 &pos, const Math::Vec2 *outline, int count, float density)
    {
        Body *body = CreateBody(BodyType::Dynamic, pos);
        body->AddMesh(outline, count, density);
        return body;
    }

    void World::Destroy(Body *body)
    {

        for (size_t i = 0; i < mJoints.size();)
        {
            Joint *joint = mJoints[i];
            if (joint->BodyA() == body || joint->BodyB() == body || joint->DependsOnBody(body))
                DestroyJointInternal(joint);
            else
                ++i;
        }

        RemoveBodyContactEvents(body);

        for (size_t i = 0; i < mContacts.size(); ++i)
        {
            ContactInfo &contact = mContacts[i];
            if (contact.a == body && contact.b->Type() == BodyType::Dynamic)
                contact.b->SetAwake(true);
            if (contact.b == body && contact.a->Type() == BodyType::Dynamic)
                contact.a->SetAwake(true);
        }
        for (auto &entry : mPairs)
        {
            Body *other = nullptr;
            if (entry.value.a == body)
                other = entry.value.b;
            else if (entry.value.b == body)
                other = entry.value.a;
            if (other && other->Type() == BodyType::Dynamic)
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

        mDeadPairs.clear();
        for (auto &entry : mPairs)
            if (entry.value.a == body || entry.value.b == body)
                mDeadPairs.push_back(entry.key);
        for (size_t i = 0; i < mDeadPairs.size(); ++i)
            mPairs.erase(mDeadPairs[i]);

        mStaleKeys.clear();
        for (auto &entry : mImpulseMap)
        {
            uint32_t idA = static_cast<uint32_t>((entry.key >> 37) & kBodyIdMask);
            uint32_t idB = static_cast<uint32_t>((entry.key >> 10) & kBodyIdMask);
            if (idA == body->mId || idB == body->mId)
                mStaleKeys.push_back(entry.key);
        }
        for (size_t i = 0; i < mStaleKeys.size(); ++i)
            mImpulseMap.erase(mStaleKeys[i]);

        for (size_t i = 0; i < mBodies.size(); ++i)
        {
            if (mBodies[i] == body)
            {
                mBodies[i] = mBodies.back();
                mBodies.pop_back();
                break;
            }
        }
        mFreeBodyIds.push_back(body->mId);
        mBodyPool.destroy(body);
    }

    void World::AddJoint(Joint *joint)
    {
        mJoints.push_back(joint);
    }

    void World::DestroyJoint(Joint *joint)
    {
        if (joint)
            DestroyJointInternal(joint);
    }

    void World::DestroyJointInternal(Joint *joint)
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

        for (size_t i = 0; i < mJoints.size();)
        {
            if (mJoints[i]->DependsOnJoint(joint))
                DestroyJointInternal(mJoints[i]);
            else
                ++i;
        }

        delete joint;
    }

    static bool ShapeContainsPoint(const Shape &shape, const Transform &xf, const Math::Vec2 &point)
    {
        if (shape.type == ShapeType::Circle)
        {
            Math::Vec2 center = xf.Transform(shape.circle.center);
            Math::Vec2 d = point - center;
            return Dot(d, d) <= shape.circle.radius * shape.circle.radius;
        }
        if (shape.type == ShapeType::Polygon)
        {
            Math::Vec2 local = InvTransformPoint(xf, point);
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

    static Math::Vec2 ClosestPointOnSegment(const Math::Vec2 &point,
                                           const Math::Vec2 &a, const Math::Vec2 &b)
    {
        Math::Vec2 edge = b - a;
        float lengthSquared = Dot(edge, edge);
        if (lengthSquared <= kEpsilon)
            return a;
        float t = Clamp(Dot(point - a, edge) / lengthSquared, 0.0f, 1.0f);
        return a + t * edge;
    }

    static Math::Vec2 ClosestPointOnShape(const Shape &shape, const Transform &xf,
                                         const Math::Vec2 &point)
    {
        if (ShapeContainsPoint(shape, xf, point))
            return point;
        if (shape.type == ShapeType::Circle)
        {
            Math::Vec2 center = xf.Transform(shape.circle.center);
            Math::Vec2 delta = point - center;
            float length = std::sqrt(Dot(delta, delta));
            return length > kEpsilon ? center + delta * (shape.circle.radius / length) : center;
        }

        Math::Vec2 closest = point;
        float bestDistance = 1.0e30f;
        int count = shape.type == ShapeType::Polygon ? shape.polygon.count : 2;
        for (int i = 0; i < count; ++i)
        {
            Math::Vec2 a;
            Math::Vec2 b;
            if (shape.type == ShapeType::Polygon)
            {
                a = xf.Transform(shape.polygon.vertices[i]);
                b = xf.Transform(shape.polygon.vertices[(i + 1) % count]);
            }
            else
            {
                a = xf.Transform(shape.edge.vertex1);
                b = xf.Transform(shape.edge.vertex2);
                if (i == 1)
                    break;
            }
            Math::Vec2 candidate = ClosestPointOnSegment(point, a, b);
            float distance = DistanceSquared(point, candidate);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                closest = candidate;
            }
        }
        return closest;
    }

    Body *World::BodyAtPoint(const Math::Vec2 &point, bool dynamicOnly) const
    {

        if (mUseTree)
        {

            const_cast<World *>(this)->SyncProxies();

            AABB pointAABB{point, point};
            ct::Vector<Body *> hits;
            BodyQueryVisitor visitor{&mTree, &hits};
            mTree.Query(&visitor, pointAABB);

            for (size_t i = 0; i < hits.size(); ++i)
            {
                Body *body = hits[i];
                if (dynamicOnly && body->Type() != BodyType::Dynamic)
                    continue;
                Transform xf = body->GetTransform();
                for (int s = 0; s < body->ShapeCount(); ++s)
                    if (ShapeContainsPoint(body->Shapes()[s], xf, point))
                        return body;
            }
            return nullptr;
        }

        for (size_t i = 0; i < mBodies.size(); ++i)
        {
            Body *body = mBodies[i];
            if (dynamicOnly && body->Type() != BodyType::Dynamic)
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

    void World::QueryAABB(const AABB &aabb, ct::Vector<Body *> &out) const
    {
        out.clear();

        if (mUseTree)
        {
            const_cast<World *>(this)->SyncProxies(); 

            ct::Vector<Body *> hits;
            BodyQueryVisitor visitor{&mTree, &hits};
            mTree.Query(&visitor, aabb);

            for (size_t i = 0; i < hits.size(); ++i)
            {
                Body *body = hits[i];
                if (body->ShapeCount() > 0 && TestOverlap(aabb, ComputeBodyAABB(*body)))
                    out.push_back(body);
            }
            return;
        }

        for (size_t i = 0; i < mBodies.size(); ++i)
        {
            Body *body = mBodies[i];
            if (body->ShapeCount() > 0 && TestOverlap(aabb, ComputeBodyAABB(*body)))
                out.push_back(body);
        }
    }

    void World::QueryCircle(const Math::Vec2 &center, float radius, ct::Vector<Body *> &out) const
    {
        out.clear();
        float radiusSquared = radius * radius;
        Math::Vec2 r(radius, radius);

        if (mUseTree)
        {
            const_cast<World *>(this)->SyncProxies(); 

            AABB queryAABB{center - r, center + r};
            ct::Vector<Body *> hits;
            BodyQueryVisitor visitor{&mTree, &hits};
            mTree.Query(&visitor, queryAABB);

            for (size_t i = 0; i < hits.size(); ++i)
            {
                Body *body = hits[i];
                if (body->ShapeCount() == 0)
                    continue;
                Transform xf = body->GetTransform();
                for (int s = 0; s < body->ShapeCount(); ++s)
                {
                    Math::Vec2 closest = ClosestPointOnShape(body->Shapes()[s], xf, center);
                    if (DistanceSquared(center, closest) <= radiusSquared)
                    {
                        out.push_back(body);
                        break;
                    }
                }
            }
            return;
        }

        for (size_t i = 0; i < mBodies.size(); ++i)
        {
            Body *body = mBodies[i];
            if (body->ShapeCount() == 0)
                continue;
            Transform xf = body->GetTransform();
            bool hit = false;
            for (int s = 0; s < body->ShapeCount(); ++s)
            {
                Math::Vec2 closest = ClosestPointOnShape(body->Shapes()[s], xf, center);
                if (DistanceSquared(center, closest) <= radiusSquared)
                {
                    hit = true;
                    break;
                }
            }
            if (hit)
                out.push_back(body);
        }
    }

    void World::RayCastGather(const Math::Vec2 &origin, const Math::Vec2 &translation,
                             uint16_t categoryMask, bool includeSensors, const Body *ignoreBody,
                             bool stopAtFirst, ct::Vector<RayCastHit> &outHits) const
    {
        outHits.clear();

        Math::Vec2 endPoint = origin + translation;
        AABB segmentAABB{Min(origin, endPoint), Max(origin, endPoint)};

        ct::Vector<Body *> candidates;
        if (mUseTree)
        {
            const_cast<World *>(this)->SyncProxies(); 

            BodyQueryVisitor visitor{&mTree, &candidates};
            mTree.Query(&visitor, segmentAABB);
        }
        else
        {
            candidates = mBodies;
        }

        float bestFraction = 1.0f;
        for (size_t i = 0; i < candidates.size(); ++i)
        {
            Body *body = candidates[i];
            if (body->ShapeCount() == 0 || body == ignoreBody)
                continue;
            Transform xf = body->GetTransform();
            for (int s = 0; s < body->ShapeCount(); ++s)
            {
                const Shape &shape = body->Shapes()[s];
                if (shape.isSensor && !includeSensors)
                    continue;
                if ((shape.filter.category & categoryMask) == 0)
                    continue;

                ShapeRayCastOutput result;
                float maxFraction = stopAtFirst ? bestFraction : 1.0f;
                if (!RayCastShape(origin, translation, maxFraction, shape, xf, result))
                    continue;

                RayCastHit hit;
                hit.body = body;
                hit.shapeIndex = s;
                hit.point = origin + result.fraction * translation;
                hit.normal = result.normal;
                hit.fraction = result.fraction;
                outHits.push_back(hit);

                if (stopAtFirst && result.fraction < bestFraction)
                    bestFraction = result.fraction;
            }
        }
    }

    bool World::RayCastClosest(const Math::Vec2 &origin, const Math::Vec2 &translation, RayCastHit &outHit,
                               uint16_t categoryMask, bool includeSensors, const Body *ignoreBody) const
    {
        RayCastGather(origin, translation, categoryMask, includeSensors, ignoreBody, true, mRayScratch);
        if (mRayScratch.empty())
            return false;

        size_t bestIndex = 0;
        for (size_t i = 1; i < mRayScratch.size(); ++i)
            if (mRayScratch[i].fraction < mRayScratch[bestIndex].fraction)
                bestIndex = i;
        outHit = mRayScratch[bestIndex];
        return true;
    }

    void World::RayCastAll(const Math::Vec2 &origin, const Math::Vec2 &translation, ct::Vector<RayCastHit> &outHits,
                           uint16_t categoryMask, bool includeSensors, const Body *ignoreBody) const
    {
        RayCastGather(origin, translation, categoryMask, includeSensors, ignoreBody, false, outHits);
    }

    void Explode(World &world, const Math::Vec2 &center, float radius, float force, float falloff)
    {
        if (radius <= 0.0f || force == 0.0f)
            return;

        ct::Vector<Body *> bodies;
        world.QueryCircle(center, radius, bodies);
        for (size_t i = 0; i < bodies.size(); ++i)
        {
            Body *body = bodies[i];
            if (body->Type() != BodyType::Dynamic)
                continue;

            Transform xf = body->GetTransform();
            Math::Vec2 point = body->WorldCenter();
            float closestDistance = 1.0e30f;
            for (int s = 0; s < body->ShapeCount(); ++s)
            {
                Math::Vec2 candidate = ClosestPointOnShape(body->Shapes()[s], xf, center);
                float candidateDistance = DistanceSquared(center, candidate);
                if (candidateDistance < closestDistance)
                {
                    closestDistance = candidateDistance;
                    point = candidate;
                }
            }
            Math::Vec2 delta = point - center;
            float distance = std::sqrt(Dot(delta, delta));
            if (distance >= radius)
                continue;
            Math::Vec2 direction = distance > kEpsilon ? delta / distance : Math::Vec2(0.0f, -1.0f);
            float amount = 1.0f - distance / radius;
            if (falloff > 0.0f)
                amount = std::pow(amount, falloff);
            body->ApplyImpulse(direction * (force * amount), point);
        }
    }

    namespace
    {
        uint64_t PairKey(const Body *a, const Body *b)
        {
            uint32_t lo = a->Id() < b->Id() ? a->Id() : b->Id();
            uint32_t hi = a->Id() < b->Id() ? b->Id() : a->Id();
            return (static_cast<uint64_t>(lo) << 32) | hi;
        }

        struct PairQueryVisitor
        {
            const DynamicTree *tree;
            int32_t queryProxyId;
            ct::Vector<int32_t> *hits;

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
    }

    void World::SyncProxies()
    {
        for (size_t i = 0; i < mBodies.size(); ++i)
        {
            Body *b = mBodies[i];
            if (b->ShapeCount() == 0)
                continue;

            AABB aabb = ComputeBodyAABB(*b);

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

    void World::FindNewPairs()
    {
        ct::Vector<int32_t> hits;

        for (size_t i = 0; i < mMoveBuffer.size(); ++i)
        {
            int32_t queryProxyId = mMoveBuffer[i];
            if (queryProxyId == kNullNode)
                continue;

            hits.clear();
            PairQueryVisitor visitor{&mTree, queryProxyId, &hits};
            mTree.Query(&visitor, mTree.GetFatAABB(queryProxyId));

            Body *self = static_cast<Body *>(mTree.GetUserData(queryProxyId));
            for (size_t h = 0; h < hits.size(); ++h)
            {
                Body *other = static_cast<Body *>(mTree.GetUserData(hits[h]));
                if (self->Type() != BodyType::Dynamic && other->Type() != BodyType::Dynamic)
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

    bool World::JointsAllowCollision(const Body *a, const Body *b) const
    {
        for (size_t i = 0; i < mJoints.size(); ++i)
        {
            Joint *joint = mJoints[i];
            Body *ja = joint->BodyA();
            Body *jb = joint->BodyB();
            if (((ja == a && jb == b) || (ja == b && jb == a)) && !joint->CollideConnected())
                return false;
        }
        return true;
    }

    void World::CollidePair(Body *first, Body *second)
    {
        if (first->mId > second->mId)
        {
            Body *tmp = first;
            first = second;
            second = tmp;
        }

        if (!JointsAllowCollision(first, second))
            return;

        Transform xfi = first->GetTransform();
        Transform xfj = second->GetTransform();

        for (int si = 0; si < first->ShapeCount(); ++si)
        {
            AABB aabbA = ComputeShapeAABB(first->Shapes()[si], xfi);
            for (int sj = 0; sj < second->ShapeCount(); ++sj)
            {
                if (!ShouldCollide(first->Shapes()[si].filter, second->Shapes()[sj].filter))
                    continue;

                AABB aabbB = ComputeShapeAABB(second->Shapes()[sj], xfj);
                if (!TestOverlap(aabbA, aabbB))
                    continue;

                Manifold manifold;
                bool flip;
                if (!CollideShapePair(manifold, flip, first->Shapes()[si], xfi, second->Shapes()[sj], xfj))
                    continue;

                if (manifold.pointCount == 0)
                    continue;

                ContactInfo info;
                info.a = flip ? second : first;
                info.b = flip ? first : second;
                info.shapeIndexA = flip ? sj : si;
                info.shapeIndexB = flip ? si : sj;
                info.manifold = manifold;
                info.sensor = first->Shapes()[si].isSensor || second->Shapes()[sj].isSensor;
                mContacts.push_back(info);
            }
        }
    }

    void World::UpdateContacts()
    {
        mContacts.clear();
        if (mUseTree)
            UpdateContactsTree();
        else
            UpdateContactsBrute();
    }

    void World::UpdateContactsTree()
    {
        SyncProxies();
        FindNewPairs();

        double n0 = mClock ? mClock() : 0.0;

        mDeadPairs.clear();
        for (auto &entry : mPairs)
        {
            Body *a = entry.value.a;
            Body *b = entry.value.b;

            if (!TestOverlap(mTree.GetFatAABB(a->mProxyId), mTree.GetFatAABB(b->mProxyId)))
            {
                mDeadPairs.push_back(entry.key);
                continue;
            }

            AABB tightA = ComputeBodyAABB(*a);
            AABB tightB = ComputeBodyAABB(*b);
            if (!TestOverlap(tightA, tightB))
                continue;

            CollidePair(a, b);
        }
        for (size_t i = 0; i < mDeadPairs.size(); ++i)
            mPairs.erase(mDeadPairs[i]);

        if (mClock)
            mNarrowMs = (float)((mClock() - n0) * 1000.0);
    }

    void World::UpdateContactsBrute()
    {
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

                CollidePair(bi, bj);
            }
        }
    }

    void World::Step(float dt)
    {
        if (dt <= 0.0f)
            return;

        ++mStepStamp;

        double t0 = mClock ? mClock() : 0.0;

        for (size_t i = 0; i < mBodies.size(); ++i)
            mBodies[i]->IntegrateVelocity(mGravity, dt);

        double t1 = mClock ? mClock() : 0.0;

        UpdateContacts();
        UpdateContactEvents();

        double t2 = mClock ? mClock() : 0.0;

        InitContactConstraints();
        WarmStartContacts();

        for (size_t i = 0; i < mJoints.size(); ++i)
            if (JointIsActive(mJoints[i]))
                mJoints[i]->InitVelocity(dt);

        for (int it = 0; it < mVelocityIterations; ++it)
        {
            for (size_t i = 0; i < mJoints.size(); ++i)
                if (JointIsActive(mJoints[i]))
                    mJoints[i]->SolveVelocity(dt);
            SolveContactVelocities();
        }

        StoreContactImpulses();

        double t3 = mClock ? mClock() : 0.0;

        mBulletSweeps.clear();
        for (size_t i = 0; i < mBodies.size(); ++i)
        {
            Body *b = mBodies[i];
            if (b->IsBullet() && b->Type() == BodyType::Dynamic && b->IsAwake())
                mBulletSweeps.push_back(BulletSweep{b, b->WorldCenter()});
        }

        for (size_t i = 0; i < mBodies.size(); ++i)
            mBodies[i]->IntegratePosition(dt);

        double t4 = mClock ? mClock() : 0.0;

        SolveContactPositions();
        SolveBulletSweeps();
        UpdateSleeping(dt);

        if (mClock)
        {
            double t5 = mClock();
            mProfile.integrate = (float)((t1 - t0 + t4 - t3) * 1000.0);
            mProfile.narrowphase = mNarrowMs;
            mProfile.broadphase = (float)((t2 - t1) * 1000.0) - mNarrowMs;
            mProfile.solveVelocity = (float)((t3 - t2) * 1000.0);
            mProfile.solvePosition = (float)((t5 - t4) * 1000.0);
        }
    }

    void World::SolveBulletSweeps()
    {
        for (size_t i = 0; i < mBulletSweeps.size(); ++i)
        {
            Body *body = mBulletSweeps[i].body;
            Math::Vec2 prevCenter = mBulletSweeps[i].prevCenter;
            Math::Vec2 newCenter = body->WorldCenter();
            Math::Vec2 delta = newCenter - prevCenter;

            float distSq = Dot(delta, delta);
            if (distSq < kLinearSlop * kLinearSlop)
                continue; 

            RayCastHit hit;
            if (!RayCastClosest(prevCenter, delta, hit, 0xFFFF, false, body))
                continue;

            if (hit.body->Type() == BodyType::Dynamic)
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

    void World::UpdateSleeping(float dt)
    {

        mTouchingActive.clear();
        for (size_t ci = 0; ci < mContacts.size(); ++ci)
        {
            const ContactInfo &c = mContacts[ci];
            if (c.sensor)
                continue;
            if (BodyIsActive(c.a) && c.b->Type() == BodyType::Dynamic)
                mTouchingActive.put(c.b, (unsigned char)1);
            if (BodyIsActive(c.b) && c.a->Type() == BodyType::Dynamic)
                mTouchingActive.put(c.a, (unsigned char)1);
        }

        float linearThresholdSquared = kSleepVelocity * kSleepVelocity;
        for (size_t i = 0; i < mBodies.size(); ++i)
        {
            Body *body = mBodies[i];
            if (body->Type() != BodyType::Dynamic)
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
            if (linearSpeedSquared > linearThresholdSquared ||
                std::fabs(body->mAngularVelocity) > kSleepAngularVelocity)
            {
                body->mSleepTime = 0.0f;
                continue;
            }

            body->mSleepTime += dt;
            if (body->mSleepTime >= kTimeToSleep)
                body->SetAwake(false);
        }
    }

    void World::SolveContactPointPosition(ContactInfo &c, int pointIndex, float baumgarte)
    {
        Body *a = c.a;
        Body *b = c.b;
        float radiusA = ShapeRadius(a->Shapes()[c.shapeIndexA]);
        float radiusB = ShapeRadius(b->Shapes()[c.shapeIndexB]);

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

    void World::SolveContactPositions()
    {
        const int kPositionIterations = 3;

        for (int it = 0; it < kPositionIterations; ++it)
        {
            for (size_t i = 0; i < mJoints.size(); ++i)
                if (JointIsActive(mJoints[i]))
                    mJoints[i]->SolvePosition();

            for (size_t ci = 0; ci < mContacts.size(); ++ci)
            {
                ContactInfo &c = mContacts[ci];
                if (c.sensor || ContactHasStatic(c) || !ContactIsActive(c))
                    continue;
                for (int i = 0; i < c.manifold.pointCount; ++i)
                    SolveContactPointPosition(c, i, kBaumgarte);
            }
            for (size_t ci = 0; ci < mContacts.size(); ++ci)
            {
                ContactInfo &c = mContacts[ci];
                if (c.sensor || !ContactHasStatic(c) || !ContactIsActive(c))
                    continue;
                for (int i = 0; i < c.manifold.pointCount; ++i)
                    SolveContactPointPosition(c, i, kBaumgarte);
            }
        }
    }

    void World::InitContactConstraints()
    {
        for (size_t ci = 0; ci < mContacts.size(); ++ci)
        {
            ContactInfo &c = mContacts[ci];
            if (c.sensor)
                continue;
            Body *a = c.a;
            Body *b = c.b;

            c.friction = sqrtf(a->mFriction * b->mFriction);
            c.restitution = a->mRestitution > b->mRestitution ? a->mRestitution : b->mRestitution;

            float radiusA = ShapeRadius(a->Shapes()[c.shapeIndexA]);
            float radiusB = ShapeRadius(b->Shapes()[c.shapeIndexB]);

            WorldManifold wm;
            wm.Initialize(&c.manifold, a->GetTransform(), radiusA, b->GetTransform(), radiusB);

            c.normal = wm.normal;
            c.tangent = Math::Vec2(-wm.normal.y, wm.normal.x);

            Math::Vec2 centerA = a->WorldCenter();
            Math::Vec2 centerB = b->WorldCenter();

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

                Math::Vec2 dv = b->mLinearVelocity + Cross(b->mAngularVelocity, c.rB[i]) -
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
            if (c.sensor || !ContactIsActive(c))
                continue;
            for (int i = 0; i < c.manifold.pointCount; ++i)
            {
                const ManifoldPoint &mp = c.manifold.points[i];
                Math::Vec2 impulse = mp.normalImpulse * c.normal + mp.tangentImpulse * c.tangent;
                c.a->mLinearVelocity -= c.a->mInvMass * impulse;
                c.a->mAngularVelocity -= c.a->mInvI * Cross(c.rA[i], impulse);
                c.b->mLinearVelocity += c.b->mInvMass * impulse;
                c.b->mAngularVelocity += c.b->mInvI * Cross(c.rB[i], impulse);
            }
        }
    }

    void World::SolveContactVelocitiesOne(ContactInfo &c)
    {
        Body *a = c.a;
        Body *b = c.b;

        for (int i = 0; i < c.manifold.pointCount; ++i)
        {
            ManifoldPoint &mp = c.manifold.points[i];

            Math::Vec2 dv = b->mLinearVelocity + Cross(b->mAngularVelocity, c.rB[i]) -
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

            Math::Vec2 impulse = lambda * c.tangent;
            a->mLinearVelocity -= a->mInvMass * impulse;
            a->mAngularVelocity -= a->mInvI * Cross(c.rA[i], impulse);
            b->mLinearVelocity += b->mInvMass * impulse;
            b->mAngularVelocity += b->mInvI * Cross(c.rB[i], impulse);
        }

        for (int i = 0; i < c.manifold.pointCount; ++i)
        {
            ManifoldPoint &mp = c.manifold.points[i];

            Math::Vec2 dv = b->mLinearVelocity + Cross(b->mAngularVelocity, c.rB[i]) -
                           a->mLinearVelocity - Cross(a->mAngularVelocity, c.rA[i]);
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

    void World::SolveContactVelocities()
    {

        for (size_t ci = 0; ci < mContacts.size(); ++ci)
        {
            ContactInfo &c = mContacts[ci];
            if (c.sensor || ContactHasStatic(c) || !ContactIsActive(c))
                continue;
            SolveContactVelocitiesOne(c);
        }
        for (size_t ci = 0; ci < mContacts.size(); ++ci)
        {
            ContactInfo &c = mContacts[ci];
            if (c.sensor || !ContactHasStatic(c) || !ContactIsActive(c))
                continue;
            SolveContactVelocitiesOne(c);
        }
    }

    void World::StoreContactImpulses()
    {
        for (size_t ci = 0; ci < mContacts.size(); ++ci)
        {
            const ContactInfo &c = mContacts[ci];
            if (c.sensor)
                continue;
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

    void World::UpdateContactEvents()
    {
        for (size_t i = 0; i < mContacts.size(); ++i)
        {
            const ContactInfo &contact = mContacts[i];
            uint64_t key = ContactKey(contact);
            ContactState *state = mContactStates.find(key);
            ContactPhase phase = state ? ContactPhase::Persist : ContactPhase::Begin;

            ContactState current;
            current.a = contact.a;
            current.b = contact.b;
            current.shapeIndexA = contact.shapeIndexA;
            current.shapeIndexB = contact.shapeIndexB;
            current.manifold = contact.manifold;
            current.sensor = contact.sensor;
            current.stamp = mStepStamp;
            mContactStates.put(key, current);
            DispatchContactEvent(phase, current);
        }

        mStaleKeys.clear();
        for (auto &entry : mContactStates)
        {
            if (entry.value.stamp != mStepStamp)
            {
                DispatchContactEvent(ContactPhase::End, entry.value);
                mStaleKeys.push_back(entry.key);
            }
        }
        for (size_t i = 0; i < mStaleKeys.size(); ++i)
            mContactStates.erase(mStaleKeys[i]);
    }

    void World::DispatchContactEvent(ContactPhase phase, const ContactState &state)
    {
        if (state.a->mContactCallback)
        {
            ContactEvent event{phase, state.a, state.b, state.shapeIndexA, state.shapeIndexB,
                               &state.manifold, state.sensor};
            state.a->mContactCallback(event, state.a->mContactContext);
        }
        if (state.b->mContactCallback)
        {
            ContactEvent event{phase, state.b, state.a, state.shapeIndexB, state.shapeIndexA,
                               &state.manifold, state.sensor};
            state.b->mContactCallback(event, state.b->mContactContext);
        }
    }

    void World::RemoveBodyContactEvents(Body *body)
    {
        mStaleKeys.clear();
        for (auto &entry : mContactStates)
        {
            if (entry.value.a == body || entry.value.b == body)
            {
                DispatchContactEvent(ContactPhase::End, entry.value);
                mStaleKeys.push_back(entry.key);
            }
        }
        for (size_t i = 0; i < mStaleKeys.size(); ++i)
            mContactStates.erase(mStaleKeys[i]);
    }

} 