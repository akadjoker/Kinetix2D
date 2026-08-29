#include "k2d/Scene.h"

#include "Collide2D.h"
#include "RayCast2D.h"
#include "ShapeCast2D.h"
#include "k2d/CanvasRenderer.h"
#include "k2d/GameObject.h"
#include "k2d/Joint2D.h"
#include "k2d/RigidBody2D.h"
#include "k2d/Steering2D.h"

#include <cmath>
#include <limits>

namespace k2d
{

namespace
{

class PhysicsRenderDebugDraw
{
public:
    struct Primitive
    {
        ct::Vector<Math::Vec2> points;
        Color color;
    };

    void drawCircleShape(const Transform& xf, float radius, Color color)
    {
        if (radius <= 0.0f)
            return;
        constexpr int segments = 32;
        Math::Vec2 points[segments];
        for (int i = 0; i < segments; ++i)
        {
            const float angle = (float)i * 6.28318530717958647692f / (float)segments;
            points[i] = xf.Transform(Math::Vec2(std::cos(angle) * radius, std::sin(angle) * radius));
        }
        addLoop(points, segments, color, 1.5f);
        addSegment(xf.Transform(Math::Vec2(0.0f, 0.0f)), xf.Transform(Math::Vec2(radius, 0.0f)), color, 1.0f);
    }

    void drawPolygonShape(const Transform& xf, const Math::Vec2* verts, int count, Color color)
    {
        if (!verts || count < 2)
            return;
        ct::Vector<Math::Vec2> points;
        points.resize((size_t)count);
        for (int i = 0; i < count; ++i)
            points[(size_t)i] = xf.Transform(verts[i]);
        addLoop(points.data(), count, color, 1.5f);
    }

    void drawSegment(const Math::Vec2& a, const Math::Vec2& b, Color color)
    {
        addSegment(a, b, color, 1.5f);
    }

    void drawPoint(const Math::Vec2& p, float size, Color color)
    {
        const float half = size > 0.0f ? size * 0.5f : 0.5f;
        Primitive& primitive = makePrimitive(color);
        primitive.points.push_back(Math::Vec2(p.x - half, p.y - half));
        primitive.points.push_back(Math::Vec2(p.x + half, p.y - half));
        primitive.points.push_back(Math::Vec2(p.x + half, p.y + half));
        primitive.points.push_back(Math::Vec2(p.x - half, p.y - half));
        primitive.points.push_back(Math::Vec2(p.x + half, p.y + half));
        primitive.points.push_back(Math::Vec2(p.x - half, p.y + half));
    }

    void drawAABB(const Math::Vec2& lower, const Math::Vec2& upper, Color color)
    {
        const Math::Vec2 points[4] = {Math::Vec2(lower.x, lower.y), Math::Vec2(upper.x, lower.y),
                                      Math::Vec2(upper.x, upper.y), Math::Vec2(lower.x, upper.y)};
        addLoop(points, 4, color, 1.0f);
    }

    void flush(RenderQueue& queue)
    {
        if (mPrimitives.empty())
            return;

        RenderItem& item = queue.AddItem(std::numeric_limits<int>::max());
        item.blendMode = BLEND_MIX;
        for (size_t i = 0; i < mPrimitives.size(); ++i)
        {
            Primitive& primitive = mPrimitives[i];
            if (primitive.points.empty())
                continue;
            RenderCommand command;
            command.type = RenderCommand::kPolygon;
            command.color = primitive.color;
            command.polygonPoints = &primitive.points;
            command.polygonPointCount = (unsigned int)primitive.points.size();
            item.commands.push_back(command);
        }
    }

private:
    Primitive& makePrimitive(const Color& color)
    {
        mPrimitives.resize(mPrimitives.size() + 1);
        Primitive& primitive = mPrimitives.back();
        primitive.color = color;
        return primitive;
    }

    void addLoop(const Math::Vec2* points, int count, const Color& color, float width)
    {
        for (int i = 0; i < count; ++i)
            addSegment(points[i], points[(i + 1) % count], color, width);
    }

    void addSegment(const Math::Vec2& a, const Math::Vec2& b, const Color& color, float width)
    {
        Math::Vec2 direction = b - a;
        const float length = direction.Length();
        if (length < 0.0001f)
            return;
        direction /= length;
        const Math::Vec2 normal(-direction.y * width * 0.5f, direction.x * width * 0.5f);
        Primitive& primitive = makePrimitive(color);
        primitive.points.push_back(a - normal);
        primitive.points.push_back(a + normal);
        primitive.points.push_back(b + normal);
        primitive.points.push_back(a - normal);
        primitive.points.push_back(b + normal);
        primitive.points.push_back(b - normal);
    }

    ct::Vector<Primitive> mPrimitives;
};

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

AABB ComputeBodyAABB(const RigidBody2D& body, const Transform& xf)
{
    AABB aabb = ComputeShapeAABB(body.Shapes()[0], xf);
    for (int i = 1; i < body.ShapeCount(); ++i)
    {
        const AABB shapeAabb = ComputeShapeAABB(body.Shapes()[(size_t)i], xf);
        aabb.lowerBound = Min(aabb.lowerBound, shapeAabb.lowerBound);
        aabb.upperBound = Max(aabb.upperBound, shapeAabb.upperBound);
    }
    return aabb;
}

bool ShapeContainsPoint(const Shape& shape, const Transform& xf, const Math::Vec2& point)
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
        const Polygon& poly = shape.polygon;
        for (int32_t i = 0; i < poly.count; ++i)
        {
            if (Dot(poly.normals[i], local - poly.vertices[i]) > 0.0f)
                return false;
        }
        return true;
    }
    return false;
}

Math::Vec2 ClosestPointOnSegment(const Math::Vec2& point, const Math::Vec2& a, const Math::Vec2& b)
{
    Math::Vec2 edge = b - a;
    float lengthSquared = Dot(edge, edge);
    if (lengthSquared <= kEpsilon)
        return a;
    float t = Clamp(Dot(point - a, edge) / lengthSquared, 0.0f, 1.0f);
    return a + t * edge;
}

Math::Vec2 ClosestPointOnShape(const Shape& shape, const Transform& xf, const Math::Vec2& point)
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

struct BodyQueryVisitor
{
    const DynamicTree* tree;
    ct::Vector<RigidBody2D*>* hits;

    bool QueryCallback(int32_t proxyId)
    {
        hits->push_back(static_cast<RigidBody2D*>(tree->GetUserData(proxyId)));
        return true;
    }
};

} // namespace

RigidBody2D* Scene::bodyAtPoint(const Math::Vec2& point, bool dynamicOnly) const
{
    if (mUseTree)
    {
        const_cast<Scene*>(this)->syncBroadphaseOnce();

        AABB pointAABB{point, point};
        ct::Vector<RigidBody2D*> hits;
        BodyQueryVisitor visitor{&mTree, &hits};
        mTree.Query(&visitor, pointAABB);

        for (size_t i = 0; i < hits.size(); ++i)
        {
            RigidBody2D* body = hits[i];
            if (dynamicOnly && body->bodyType() != BodyType::Dynamic)
                continue;
            Transform xf = body->GetTransform();
            for (int s = 0; s < body->ShapeCount(); ++s)
                if (ShapeContainsPoint(body->Shapes()[(size_t)s], xf, point))
                    return body;
        }
        return nullptr;
    }

    for (size_t i = 0; i < mBodies.size(); ++i)
    {
        RigidBody2D* body = mBodies[i];
        if (dynamicOnly && body->bodyType() != BodyType::Dynamic)
            continue;
        Transform xf = body->GetTransform();
        for (int s = 0; s < body->ShapeCount(); ++s)
            if (ShapeContainsPoint(body->Shapes()[(size_t)s], xf, point))
                return body;
    }
    return nullptr;
}

void Scene::queryAABB(const AABB& aabb, ct::Vector<RigidBody2D*>& out) const
{
    out.clear();

    if (mUseTree)
    {
        const_cast<Scene*>(this)->syncBroadphaseOnce();

        ct::Vector<RigidBody2D*> hits;
        BodyQueryVisitor visitor{&mTree, &hits};
        mTree.Query(&visitor, aabb);

        for (size_t i = 0; i < hits.size(); ++i)
        {
            RigidBody2D* body = hits[i];
            if (body->ShapeCount() > 0 && TestOverlap(aabb, ComputeBodyAABB(*body)))
                out.push_back(body);
        }
        return;
    }

    for (size_t i = 0; i < mBodies.size(); ++i)
    {
        RigidBody2D* body = mBodies[i];
        if (body->ShapeCount() > 0 && TestOverlap(aabb, ComputeBodyAABB(*body)))
            out.push_back(body);
    }
}

void Scene::queryCircle(const Math::Vec2& center, float radius, ct::Vector<RigidBody2D*>& out) const
{
    out.clear();
    float radiusSquared = radius * radius;
    Math::Vec2 r(radius, radius);

    if (mUseTree)
    {
        const_cast<Scene*>(this)->syncBroadphaseOnce();

        AABB queryAabb{center - r, center + r};
        ct::Vector<RigidBody2D*> hits;
        BodyQueryVisitor visitor{&mTree, &hits};
        mTree.Query(&visitor, queryAabb);

        for (size_t i = 0; i < hits.size(); ++i)
        {
            RigidBody2D* body = hits[i];
            if (body->ShapeCount() == 0)
                continue;
            Transform xf = body->GetTransform();
            for (int s = 0; s < body->ShapeCount(); ++s)
            {
                Math::Vec2 closest = ClosestPointOnShape(body->Shapes()[(size_t)s], xf, center);
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
        RigidBody2D* body = mBodies[i];
        if (body->ShapeCount() == 0)
            continue;
        Transform xf = body->GetTransform();
        bool hit = false;
        for (int s = 0; s < body->ShapeCount(); ++s)
        {
            Math::Vec2 closest = ClosestPointOnShape(body->Shapes()[(size_t)s], xf, center);
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

void Scene::rayCastGather(const Math::Vec2& origin, const Math::Vec2& translation, uint16_t categoryMask,
                          bool includeSensors, const RigidBody2D* ignoreBody, bool stopAtFirst,
                          ct::Vector<RayCastHit>& outHits) const
{
    outHits.clear();

    Math::Vec2 endPoint = origin + translation;
    AABB segmentAABB{Min(origin, endPoint), Max(origin, endPoint)};

    mBodyScratch.clear();
    if (mUseTree)
    {
        const_cast<Scene*>(this)->syncBroadphaseOnce();

        BodyQueryVisitor visitor{&mTree, &mBodyScratch};
        mTree.Query(&visitor, segmentAABB);
    }
    else
    {
        mBodyScratch = mBodies;
    }

    float bestFraction = 1.0f;
    for (size_t i = 0; i < mBodyScratch.size(); ++i)
    {
        RigidBody2D* body = mBodyScratch[i];
        if (body->ShapeCount() == 0 || body == ignoreBody)
            continue;
        Transform xf = body->GetTransform();
        for (int s = 0; s < body->ShapeCount(); ++s)
        {
            const Shape& shape = body->Shapes()[(size_t)s];
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

bool Scene::rayCastClosest(const Math::Vec2& origin, const Math::Vec2& translation, RayCastHit& outHit,
                           uint16_t categoryMask, bool includeSensors, const RigidBody2D* ignoreBody) const
{
    rayCastGather(origin, translation, categoryMask, includeSensors, ignoreBody, true, mRayScratch);
    if (mRayScratch.empty())
        return false;

    size_t bestIndex = 0;
    for (size_t i = 1; i < mRayScratch.size(); ++i)
        if (mRayScratch[i].fraction < mRayScratch[bestIndex].fraction)
            bestIndex = i;
    outHit = mRayScratch[bestIndex];
    return true;
}

Math::Vec2 Scene::recoverOverlap(RigidBody2D& body, const Math::Vec2& position, float margin) const
{
    Math::Vec2 recovery(0.0f, 0.0f);
    if (body.ShapeCount() == 0)
        return recovery;

    const float target = margin > kLinearSlop ? margin : kLinearSlop;
    const ct::Vector<Shape>& selfShapes = body.Shapes();
    for (int pass = 0; pass < 4; ++pass)
    {
        const Transform selfTransform = MakeTransform(position + recovery, body.Angle());
        const AABB bounds = ComputeBodyAABB(body, selfTransform);
        mBodyScratch.clear();
        queryAABB(bounds, mBodyScratch);

        Math::Vec2 push(0.0f, 0.0f);
        int contacts = 0;
        for (size_t candidateIndex = 0; candidateIndex < mBodyScratch.size(); ++candidateIndex)
        {
            RigidBody2D* other = mBodyScratch[candidateIndex];
            if (!other || other == &body || other->ShapeCount() == 0)
                continue;
            const Transform otherTransform = other->GetTransform();
            const ct::Vector<Shape>& otherShapes = other->Shapes();
            for (int selfIndex = 0; selfIndex < body.ShapeCount(); ++selfIndex)
            {
                const Shape& selfShape = selfShapes[(size_t)selfIndex];
                const AABB selfAabb = ComputeShapeAABB(selfShape, selfTransform);
                for (int otherIndex = 0; otherIndex < other->ShapeCount(); ++otherIndex)
                {
                    const Shape& otherShape = otherShapes[(size_t)otherIndex];
                    if (selfShape.isSensor || otherShape.isSensor ||
                        !ShouldCollide(selfShape.filter, otherShape.filter) ||
                        !TestOverlap(selfAabb, ComputeShapeAABB(otherShape, otherTransform)))
                        continue;

                    Manifold manifold;
                    bool flipped = false;
                    if (!CollideShapePair(manifold, flipped, selfShape, selfTransform, otherShape, otherTransform) ||
                        manifold.pointCount == 0)
                        continue;

                    WorldManifold worldManifold;
                    Math::Vec2 normal;
                    if (flipped)
                    {
                        worldManifold.Initialize(&manifold, otherTransform, ShapeRadius(otherShape), selfTransform,
                                                 ShapeRadius(selfShape));
                        normal = worldManifold.normal;
                    }
                    else
                    {
                        worldManifold.Initialize(&manifold, selfTransform, ShapeRadius(selfShape), otherTransform,
                                                 ShapeRadius(otherShape));
                        normal = -worldManifold.normal;
                    }

                    float depth = 0.0f;
                    for (int32_t pointIndex = 0; pointIndex < manifold.pointCount; ++pointIndex)
                    {
                        const float separation = -worldManifold.separations[pointIndex];
                        if (separation > depth)
                            depth = separation;
                    }
                    if (depth <= 0.0f)
                        continue;

                    push += normal * (depth + target * 0.25f);
                    ++contacts;
                }
            }
        }

        if (contacts == 0)
            break;
        recovery += push / (float)contacts;
    }
    return recovery;
}

bool Scene::testMotion(RigidBody2D& body, const Math::Vec2& motion, MotionResult& out, float safeMargin) const
{
    out = MotionResult();
    if (body.ShapeCount() == 0)
    {
        out.travel = motion;
        return false;
    }

    const Math::Vec2 recovery = recoverOverlap(body, body.Position(), safeMargin);
    const Transform selfTransform = MakeTransform(body.Position() + recovery, body.Angle());
    const AABB start = ComputeBodyAABB(body, selfTransform);
    Transform endTransform = selfTransform;
    endTransform.tx += motion.x;
    endTransform.ty += motion.y;
    const AABB end = ComputeBodyAABB(body, endTransform);
    const float margin = safeMargin > 0.0f ? safeMargin : 0.0f;
    const Math::Vec2 expansion(margin, margin);
    const AABB sweep{Min(start.lowerBound, end.lowerBound) - expansion,
                     Max(start.upperBound, end.upperBound) + expansion};

    mBodyScratch.clear();
    queryAABB(sweep, mBodyScratch);

    ShapeCastOutput best;
    best.fraction = 1.0f;
    int bestSelfShape = -1;
    int bestOtherShape = -1;
    RigidBody2D* bestBody = nullptr;
    const ct::Vector<Shape>& selfShapes = body.Shapes();
    for (size_t candidateIndex = 0; candidateIndex < mBodyScratch.size(); ++candidateIndex)
    {
        RigidBody2D* other = mBodyScratch[candidateIndex];
        if (!other || other == &body || other->ShapeCount() == 0)
            continue;
        const Transform otherTransform = other->GetTransform();
        const ct::Vector<Shape>& otherShapes = other->Shapes();
        for (int selfIndex = 0; selfIndex < body.ShapeCount(); ++selfIndex)
        {
            const AABB selfSweep = ComputeShapeAABB(selfShapes[(size_t)selfIndex], selfTransform);
            for (int otherIndex = 0; otherIndex < other->ShapeCount(); ++otherIndex)
            {
                const Shape& otherShape = otherShapes[(size_t)otherIndex];
                if ((!false && (selfShapes[(size_t)selfIndex].isSensor || otherShape.isSensor)) ||
                    !ShouldCollide(selfShapes[(size_t)selfIndex].filter, otherShape.filter))
                    continue;

                AABB pairSweep{Min(selfSweep.lowerBound, selfSweep.lowerBound + motion) - expansion,
                               Max(selfSweep.upperBound, selfSweep.upperBound + motion) + expansion};
                if (!TestOverlap(pairSweep, ComputeShapeAABB(otherShape, otherTransform)))
                    continue;

                ShapeCastInput cast;
                cast.shapeA = &otherShape;
                cast.transformA = otherTransform;
                cast.shapeB = &selfShapes[(size_t)selfIndex];
                cast.transformB = selfTransform;
                cast.translationB = motion;
                ShapeCastOutput hit;
                if (!ShapeCast(cast, hit) || hit.fraction >= best.fraction)
                    continue;

                // A->B is the shape-cast normal. B is the moving body, so it
                // already points out of the obstacle.
                best = hit;
                bestSelfShape = selfIndex;
                bestOtherShape = otherIndex;
                bestBody = other;
            }
        }
    }

    if (!bestBody)
    {
        out.travel = recovery + motion;
        return false;
    }

    const float length = std::sqrt(Dot(motion, motion));
    const float safeFraction = length > kEpsilon ? Clamp(best.fraction - margin / length, 0.0f, 1.0f) : 0.0f;
    out.body = bestBody;
    out.shapeIndexSelf = bestSelfShape;
    out.shapeIndexOther = bestOtherShape;
    out.point = best.point;
    out.normal = best.normal;
    out.fraction = best.fraction;
    out.travel = recovery + safeFraction * motion;
    out.remainder = motion - safeFraction * motion;
    out.hit = true;
    return true;
}

bool Scene::testPosition(RigidBody2D& body, const Math::Vec2& position, MotionResult& out) const
{
    out = MotionResult();
    if (body.ShapeCount() == 0)
        return false;

    const Transform selfTransform = MakeTransform(position, body.Angle());
    const AABB bounds = ComputeBodyAABB(body, selfTransform);
    mBodyScratch.clear();
    queryAABB(bounds, mBodyScratch);
    const ct::Vector<Shape>& selfShapes = body.Shapes();
    for (size_t candidateIndex = 0; candidateIndex < mBodyScratch.size(); ++candidateIndex)
    {
        RigidBody2D* other = mBodyScratch[candidateIndex];
        if (!other || other == &body || other->ShapeCount() == 0)
            continue;
        const Transform otherTransform = other->GetTransform();
        const ct::Vector<Shape>& otherShapes = other->Shapes();
        for (int selfIndex = 0; selfIndex < body.ShapeCount(); ++selfIndex)
        {
            const AABB selfAabb = ComputeShapeAABB(selfShapes[(size_t)selfIndex], selfTransform);
            for (int otherIndex = 0; otherIndex < other->ShapeCount(); ++otherIndex)
            {
                const Shape& otherShape = otherShapes[(size_t)otherIndex];
                if ((selfShapes[(size_t)selfIndex].isSensor || otherShape.isSensor) ||
                    !ShouldCollide(selfShapes[(size_t)selfIndex].filter, otherShape.filter) ||
                    !TestOverlap(selfAabb, ComputeShapeAABB(otherShape, otherTransform)))
                    continue;

                Manifold manifold;
                bool flipped = false;
                if (!CollideShapePair(manifold, flipped, selfShapes[(size_t)selfIndex], selfTransform, otherShape,
                                      otherTransform) ||
                    manifold.pointCount == 0)
                    continue;

                WorldManifold worldManifold;
                if (flipped)
                {
                    worldManifold.Initialize(&manifold, otherTransform, ShapeRadius(otherShape), selfTransform,
                                             ShapeRadius(selfShapes[(size_t)selfIndex]));
                    out.normal = worldManifold.normal;
                }
                else
                {
                    worldManifold.Initialize(&manifold, selfTransform, ShapeRadius(selfShapes[(size_t)selfIndex]),
                                             otherTransform, ShapeRadius(otherShape));
                    out.normal = -worldManifold.normal;
                }
                out.body = other;
                out.shapeIndexSelf = selfIndex;
                out.shapeIndexOther = otherIndex;
                out.point = worldManifold.points[0];
                out.fraction = 0.0f;
                out.hit = true;
                return true;
            }
        }
    }
    return false;
}

GameObject* Scene::raycast(const Math::Vec2& origin, const Math::Vec2& direction, float distance,
                           Math::Vec2* outPoint, Math::Vec2* outNormal, const GameObject* ignore,
                           uint16_t categoryMask)
{
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length < 0.0001f || distance <= 0.0f)
        return nullptr;

    const Math::Vec2 translation(direction.x / length * distance, direction.y / length * distance);

    const RigidBody2D* ignoreBody = ignore ? ignore->getComponent<RigidBody2D>() : nullptr;

    RayCastHit hit;
    if (!rayCastClosest(origin, translation, hit, categoryMask, false, ignoreBody))
        return nullptr;

    if (outPoint)
        *outPoint = hit.point;
    if (outNormal)
        *outNormal = hit.normal;
    return objectForBody(hit.body);
}

GameObject* Scene::objectAtPoint(const Math::Vec2& point)
{
    return objectForBody(bodyAtPoint(point, false));
}

void Scene::overlapCircle(const Math::Vec2& center, float radius, ct::Vector<GameObject*>& out)
{
    out.clear();
    if (radius <= 0.0f)
        return;

    ct::Vector<RigidBody2D*> bodies;
    queryCircle(center, radius, bodies);
    for (size_t i = 0; i < bodies.size(); ++i)
        if (GameObject* object = objectForBody(bodies[i]))
            out.push_back(object);
}

GameObject* Scene::objectForBody(const RigidBody2D* body)
{
    return body ? const_cast<RigidBody2D*>(body)->owner() : nullptr;
}

void Scene::syncBroadphaseOnce() const
{
    if (!mUseTree || mBroadphaseStamp == mFrameStamp)
        return;
    const_cast<Scene*>(this)->syncProxies();
    mBroadphaseStamp = mFrameStamp;
}

Steering2D* Scene::steeringAt(std::size_t index) const
{
    return index < mSteerings.size() ? mSteerings[index] : nullptr;
}

Math::Vec2 Scene::steeringForce(const GameObject& object, const Math::Vec2& velocity, float deltaTime,
                                bool* outVetoed) const
{
    Math::Vec2 total(0.0f, 0.0f);
    Math::Vec2 veto(0.0f, 0.0f);
    bool vetoed = false;
    const Math::Vec2 position = object.globalPosition();
    for (Component* component = object.mComponents[static_cast<uint8_t>(ComponentType::Steering)]; component;
         component = component->mNextSibling)
    {
        if (!component->active())
            continue;
        Steering2D* steering = static_cast<Steering2D*>(component);
        bool steeringVetoed = false;
        const Math::Vec2 contribution = steering->force(deltaTime, position, velocity, steeringVetoed);
        if (!std::isfinite(contribution.x) || !std::isfinite(contribution.y))
            continue;
        if (steeringVetoed)
        {
            veto += contribution;
            vetoed = true;
            continue;
        }
        total += contribution;
    }
    if (outVetoed)
        *outVetoed = vetoed;
    return vetoed ? veto : total;
}

std::size_t Scene::queryNeighbours(const GameObject& self, const Math::Vec2& center, float radius,
                                   uint16_t mask) const
{
    mNeighbours.clear();
    if (radius <= 0.0f || mask == 0)
        return 0;

    const float radiusSquared = radius * radius;
    const Math::Vec2 extent(radius, radius);
    const AABB bounds{center - extent, center + extent};

    mNeighbourBodies.clear();
    if (mUseTree)
    {
        syncBroadphaseOnce();
        BodyQueryVisitor visitor{&mTree, &mNeighbourBodies};
        mTree.Query(&visitor, bounds);
    }
    else
    {
        mNeighbourBodies = mBodies;
    }

    for (size_t i = 0; i < mNeighbourBodies.size(); ++i)
    {
        RigidBody2D* body = mNeighbourBodies[i];
        GameObject* object = body ? body->owner() : nullptr;
        if (!object || object == &self)
            continue;

        const Transform xf = body->GetTransform();
        for (int s = 0; s < body->ShapeCount(); ++s)
        {
            // A trigger volume is not crowd: raycast already skips sensors, so
            // separation must too or the two behaviours disagree on the same shape.
            if (body->IsSensor(s))
                continue;
            if ((body->ShapeFilter(s).category & mask) == 0)
                continue;
            const Math::Vec2 closest = ClosestPointOnShape(body->Shapes()[(size_t)s], xf, center);
            if (DistanceSquared(center, closest) > radiusSquared)
                continue;
            mNeighbours.push_back(object);
            break;
        }
    }
    return mNeighbours.size();
}

GameObject* Scene::neighbourAt(std::size_t index) const
{
    return index < mNeighbours.size() ? mNeighbours[index] : nullptr;
}

void Scene::debugDrawBodies(CanvasRenderer& canvas, unsigned flags)
{
    if (flags == 0)
        return;

    PhysicsRenderDebugDraw draw;

    if (flags & DebugDrawJoints)
    {
        for (size_t i = 0; i < mJoints.size(); ++i)
        {
            if (!mJoints[i]->isConnected())
                continue;
            Math::Vec2 a = mJoints[i]->anchorA();
            Math::Vec2 b = mJoints[i]->anchorB();
            Color jointColor = Color::FromBytes(120, 200, 255, 255);
            draw.drawSegment(a, b, jointColor);
            draw.drawPoint(a, 3.0f, jointColor);
            draw.drawPoint(b, 3.0f, jointColor);
        }
    }

    const auto shapeColorForBody = [](const RigidBody2D& body) -> Color
    {
        if (body.bodyType() == BodyType::Dynamic)
            return Color::FromBytes(80, 220, 120, 255);
        if (body.bodyType() == BodyType::Kinematic)
            return Color::FromBytes(80, 160, 255, 255);
        return Color::FromBytes(140, 140, 140, 255);
    };

    if (flags & DebugDrawShapes)
    {
        for (size_t i = 0; i < mBodies.size(); ++i)
        {
            RigidBody2D& body = *mBodies[i];
            Color color = shapeColorForBody(body);
            Transform xf = body.GetTransform();
            for (int s = 0; s < body.ShapeCount(); ++s)
            {
                const Shape& shape = body.Shapes()[(size_t)s];
                Color shapeColor = shape.isSensor ? Color::FromBytes(80, 180, 255, 255) : color;
                switch (shape.type)
                {
                case ShapeType::Circle:
                    draw.drawCircleShape(xf, shape.circle.radius, shapeColor);
                    break;
                case ShapeType::Polygon:
                    draw.drawPolygonShape(xf, shape.polygon.vertices, shape.polygon.count, shapeColor);
                    break;
                case ShapeType::Edge:
                    draw.drawSegment(xf.Transform(shape.edge.vertex1), xf.Transform(shape.edge.vertex2), shapeColor);
                    break;
                }
            }
            Math::Vec2 origin = xf.Transform(Math::Vec2(0.0f, 0.0f));
            Math::Vec2 tip = xf.Transform(Math::Vec2(20.0f, 0.0f));
            draw.drawSegment(origin, tip, Color::FromBytes(255, 255, 255, 255));
        }
    }

    if (flags & DebugDrawAABBs)
    {
        for (size_t i = 0; i < mBodies.size(); ++i)
        {
            RigidBody2D& body = *mBodies[i];
            if (body.ShapeCount() == 0)
                continue;
            Transform xf = body.GetTransform();
            AABB aabb = ComputeShapeAABB(body.Shapes()[0], xf);
            for (int s = 1; s < body.ShapeCount(); ++s)
            {
                AABB shapeAabb = ComputeShapeAABB(body.Shapes()[(size_t)s], xf);
                aabb.lowerBound = Min(aabb.lowerBound, shapeAabb.lowerBound);
                aabb.upperBound = Max(aabb.upperBound, shapeAabb.upperBound);
            }
            draw.drawAABB(aabb.lowerBound, aabb.upperBound, Color::FromBytes(200, 200, 60, 160));
        }
    }

    if (flags & DebugDrawContacts)
    {
        for (size_t i = 0; i < mContacts.size(); ++i)
        {
            const ContactInfo& c = mContacts[i];
            float radiusA = ShapeRadius(c.a->Shapes()[(size_t)c.shapeIndexA]);
            float radiusB = ShapeRadius(c.b->Shapes()[(size_t)c.shapeIndexB]);
            WorldManifold worldManifold;
            worldManifold.Initialize(&c.manifold, c.a->GetTransform(), radiusA, c.b->GetTransform(), radiusB);
            const Color pointColor = Color::FromBytes(255, 40, 40, 255);
            const Color normalColor = Color::FromBytes(255, 140, 40, 255);
            const float arrowLength = 15.0f;
            for (int32_t p = 0; p < c.manifold.pointCount; ++p)
            {
                draw.drawPoint(worldManifold.points[p], 4.0f, pointColor);
                Math::Vec2 tip = worldManifold.points[p] + arrowLength * worldManifold.normal;
                draw.drawSegment(worldManifold.points[p], tip, normalColor);
            }
        }
    }

    RenderQueue queue;
    draw.flush(queue);
    queue.Flush(canvas);
}

}
