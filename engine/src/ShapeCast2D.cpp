// Shape-cast implementation adapted from Box2D's b2ShapeCast (MIT License,
// Copyright 2019 Erin Catto). The original uses GJK raycasting and is kept
// algorithmically equivalent here; only vector/shape wrappers differ.

#include "ShapeCast2D.h"


#include <cmath>

namespace k2d
{
namespace
{
struct Proxy
{
    Math::Vec2 vertices[kMaxPolygonVertices];
    int count = 0;
    float radius = 0.0f;

    bool set(const Shape& shape)
    {
        radius = ShapeRadius(shape);
        switch (shape.type)
        {
        case ShapeType::Circle:
            vertices[0] = shape.circle.center;
            count = 1;
            return true;
        case ShapeType::Polygon:
            count = shape.polygon.count;
            for (int i = 0; i < count; ++i)
                vertices[i] = shape.polygon.vertices[i];
            return count > 0;
        case ShapeType::Edge:
            vertices[0] = shape.edge.vertex1;
            vertices[1] = shape.edge.vertex2;
            count = 2;
            return true;
        }
        return false;
    }

    int support(const Math::Vec2& direction) const
    {
        int best = 0;
        float value = Dot(vertices[0], direction);
        for (int i = 1; i < count; ++i)
        {
            const float candidate = Dot(vertices[i], direction);
            if (candidate > value)
            {
                value = candidate;
                best = i;
            }
        }
        return best;
    }
};

struct SimplexVertex
{
    Math::Vec2 wA;
    Math::Vec2 wB;
    Math::Vec2 w;
    float a = 0.0f;
    int indexA = 0;
    int indexB = 0;
};

struct Simplex
{
    SimplexVertex v[3];
    int count = 0;

    Math::Vec2 searchDirection() const
    {
        if (count == 1)
            return -v[0].w;
        const Math::Vec2 edge = v[1].w - v[0].w;
        return Cross(edge, -v[0].w) > 0.0f ? Cross(1.0f, edge) : Cross(edge, 1.0f);
    }

    Math::Vec2 closestPoint() const
    {
        if (count == 1)
            return v[0].w;
        if (count == 2)
            return v[0].a * v[0].w + v[1].a * v[1].w;
        return Math::Vec2(0.0f, 0.0f);
    }

    void witnessPoints(Math::Vec2& a, Math::Vec2& b) const
    {
        if (count == 1)
        {
            a = v[0].wA;
            b = v[0].wB;
        }
        else if (count == 2)
        {
            a = v[0].a * v[0].wA + v[1].a * v[1].wA;
            b = v[0].a * v[0].wB + v[1].a * v[1].wB;
        }
        else
        {
            a = v[0].a * v[0].wA + v[1].a * v[1].wA + v[2].a * v[2].wA;
            b = a;
        }
    }

    void solve2()
    {
        const Math::Vec2 e = v[1].w - v[0].w;
        const float d2 = -Dot(v[0].w, e);
        if (d2 <= 0.0f)
        {
            v[0].a = 1.0f;
            count = 1;
            return;
        }
        const float d1 = Dot(v[1].w, e);
        if (d1 <= 0.0f)
        {
            v[1].a = 1.0f;
            v[0] = v[1];
            count = 1;
            return;
        }
        const float inv = 1.0f / (d1 + d2);
        v[0].a = d1 * inv;
        v[1].a = d2 * inv;
        count = 2;
    }

    void solve3()
    {
        const Math::Vec2 w1 = v[0].w;
        const Math::Vec2 w2 = v[1].w;
        const Math::Vec2 w3 = v[2].w;
        const Math::Vec2 e12 = w2 - w1;
        const Math::Vec2 e13 = w3 - w1;
        const Math::Vec2 e23 = w3 - w2;
        const float d12_1 = Dot(w2, e12), d12_2 = -Dot(w1, e12);
        const float d13_1 = Dot(w3, e13), d13_2 = -Dot(w1, e13);
        const float d23_1 = Dot(w3, e23), d23_2 = -Dot(w2, e23);
        const float n123 = Cross(e12, e13);
        const float d123_1 = n123 * Cross(w2, w3);
        const float d123_2 = n123 * Cross(w3, w1);
        const float d123_3 = n123 * Cross(w1, w2);

        if (d12_2 <= 0.0f && d13_2 <= 0.0f)
        {
            v[0].a = 1.0f;
            count = 1;
            return;
        }
        if (d12_1 > 0.0f && d12_2 > 0.0f && d123_3 <= 0.0f)
        {
            const float inv = 1.0f / (d12_1 + d12_2);
            v[0].a = d12_1 * inv;
            v[1].a = d12_2 * inv;
            count = 2;
            return;
        }
        if (d13_1 > 0.0f && d13_2 > 0.0f && d123_2 <= 0.0f)
        {
            const float inv = 1.0f / (d13_1 + d13_2);
            v[0].a = d13_1 * inv;
            v[2].a = d13_2 * inv;
            v[1] = v[2];
            count = 2;
            return;
        }
        if (d12_1 <= 0.0f && d23_2 <= 0.0f)
        {
            v[1].a = 1.0f;
            v[0] = v[1];
            count = 1;
            return;
        }
        if (d13_1 <= 0.0f && d23_1 <= 0.0f)
        {
            v[2].a = 1.0f;
            v[0] = v[2];
            count = 1;
            return;
        }
        if (d23_1 > 0.0f && d23_2 > 0.0f && d123_1 <= 0.0f)
        {
            const float inv = 1.0f / (d23_1 + d23_2);
            v[1].a = d23_1 * inv;
            v[2].a = d23_2 * inv;
            v[0] = v[2];
            count = 2;
            return;
        }
        const float inv = 1.0f / (d123_1 + d123_2 + d123_3);
        v[0].a = d123_1 * inv;
        v[1].a = d123_2 * inv;
        v[2].a = d123_3 * inv;
        count = 3;
    }
};
} // namespace

bool ShapeCast(const ShapeCastInput& input, ShapeCastOutput& output)
{
    output = ShapeCastOutput();
    if (!input.shapeA || !input.shapeB)
        return false;

    Proxy proxyA, proxyB;
    if (!proxyA.set(*input.shapeA) || !proxyB.set(*input.shapeB))
        return false;

    const float radiusA = proxyA.radius > kPolygonRadius ? proxyA.radius : kPolygonRadius;
    const float radiusB = proxyB.radius > kPolygonRadius ? proxyB.radius : kPolygonRadius;
    const float sigma =
        (radiusA + radiusB - kPolygonRadius) > kPolygonRadius ? (radiusA + radiusB - kPolygonRadius) : kPolygonRadius;
    const float tolerance = 0.5f * kLinearSlop;

    const Math::Vec2 translation = input.translationB;
    Simplex simplex;
    int indexA = proxyA.support(InvRotate(input.transformA, -translation));
    int indexB = proxyB.support(InvRotate(input.transformB, translation));
    Math::Vec2 wA = input.transformA.Transform(proxyA.vertices[indexA]);
    Math::Vec2 wB = input.transformB.Transform(proxyB.vertices[indexB]);
    Math::Vec2 v = wA - wB;
    Math::Vec2 normal(0.0f, 0.0f);
    float fraction = 0.0f;

    for (int iteration = 0; iteration < 20 && std::sqrt(Dot(v, v)) - sigma > tolerance; ++iteration)
    {
        output.iterations = iteration + 1;
        indexA = proxyA.support(InvRotate(input.transformA, -v));
        wA = input.transformA.Transform(proxyA.vertices[indexA]);
        indexB = proxyB.support(InvRotate(input.transformB, v));
        wB = input.transformB.Transform(proxyB.vertices[indexB]);
        const Math::Vec2 p = wA - wB;
        const Math::Vec2 unitV = Normalize(v);
        const float vp = Dot(unitV, p);
        const float vr = Dot(unitV, translation);
        if (vp - sigma > fraction * vr)
        {
            if (vr <= 0.0f)
                return false;
            fraction = (vp - sigma) / vr;
            if (fraction > 1.0f)
                return false;
            normal = -unitV;
            simplex.count = 0;
        }

        SimplexVertex& vertex = simplex.v[simplex.count++];
        vertex.indexA = indexB;
        vertex.wA = wB + fraction * translation;
        vertex.indexB = indexA;
        vertex.wB = wA;
        vertex.w = vertex.wB - vertex.wA;
        vertex.a = 1.0f;

        if (simplex.count == 2)
            simplex.solve2();
        else if (simplex.count == 3)
            simplex.solve3();
        if (simplex.count == 3)
            return false;
        v = simplex.closestPoint();
    }

    if (output.iterations == 0)
        return false;

    Math::Vec2 pointA, pointB;
    simplex.witnessPoints(pointB, pointA);
    if (Dot(v, v) > 0.0f)
        normal = Normalize(-v);
    output.point = pointA + radiusA * normal;
    output.normal = normal;
    output.fraction = fraction;
    return true;
}
} // namespace k2d
