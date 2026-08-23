#include "kx/shapes.h"

#include <cassert>
#include <cmath>

namespace kx
{

    AABB Circle::ComputeAABB(const Transform &xf) const
    {
        Math::Vec2 p = xf.Transform(center);
        AABB aabb;
        aabb.lowerBound = Math::Vec2(p.x - radius, p.y - radius);
        aabb.upperBound = Math::Vec2(p.x + radius, p.y + radius);
        return aabb;
    }

    MassData Circle::ComputeMass(float density) const
    {
        MassData massData;
        massData.mass = density * kPi * radius * radius;
        massData.center = center;
        massData.I = massData.mass * (0.5f * radius * radius + Dot(center, center));
        return massData;
    }

    void Edge::SetTwoSided(const Math::Vec2 &v1, const Math::Vec2 &v2)
    {
        vertex1 = v1;
        vertex2 = v2;
        oneSided = false;
    }

    AABB Edge::ComputeAABB(const Transform &xf) const
    {
        Math::Vec2 v1 = xf.Transform(vertex1);
        Math::Vec2 v2 = xf.Transform(vertex2);

        Math::Vec2 lower = Min(v1, v2);
        Math::Vec2 upper = Max(v1, v2);

        Math::Vec2 r(radius, radius);
        AABB aabb;
        aabb.lowerBound = lower - r;
        aabb.upperBound = upper + r;
        return aabb;
    }

    MassData Edge::ComputeMass(float) const
    {
        MassData massData;
        massData.mass = 0.0f;
        massData.center = 0.5f * (vertex1 + vertex2);
        massData.I = 0.0f;
        return massData;
    }

    void Polygon::SetAsBox(float halfWidth, float halfHeight)
    {
        count = 4;
        vertices[0] = Math::Vec2(-halfWidth, -halfHeight);
        vertices[1] = Math::Vec2(halfWidth, -halfHeight);
        vertices[2] = Math::Vec2(halfWidth, halfHeight);
        vertices[3] = Math::Vec2(-halfWidth, halfHeight);
        normals[0] = Math::Vec2(0.0f, -1.0f);
        normals[1] = Math::Vec2(1.0f, 0.0f);
        normals[2] = Math::Vec2(0.0f, 1.0f);
        normals[3] = Math::Vec2(-1.0f, 0.0f);
        centroid = Math::Vec2(0.0f, 0.0f);
    }

    void Polygon::SetAsBox(float halfWidth, float halfHeight, const Math::Vec2 &boxCenter, float angle)
    {
        SetAsBox(halfWidth, halfHeight);
        centroid = boxCenter;

        Transform xf = MakeTransform(boxCenter, angle);

        for (int32_t i = 0; i < count; ++i)
        {
            vertices[i] = xf.Transform(vertices[i]);
            normals[i] = Math::Vec2(xf.a * normals[i].x + xf.c * normals[i].y,
                                    xf.b * normals[i].x + xf.d * normals[i].y);
        }
    }

    namespace
    {

        Math::Vec2 ComputePolygonCentroid(const Math::Vec2 *vs, int32_t vertCount)
        {
            assert(vertCount >= 3);

            Math::Vec2 c(0.0f, 0.0f);
            float area = 0.0f;

            Math::Vec2 s = vs[0];
            const float inv3 = 1.0f / 3.0f;

            for (int32_t i = 0; i < vertCount; ++i)
            {
                Math::Vec2 p1 = vs[0] - s;
                Math::Vec2 p2 = vs[i] - s;
                Math::Vec2 p3 = i + 1 < vertCount ? vs[i + 1] - s : vs[0] - s;

                Math::Vec2 e1 = p2 - p1;
                Math::Vec2 e2 = p3 - p1;

                float triangleArea = 0.5f * Cross(e1, e2);
                area += triangleArea;
                c += triangleArea * inv3 * (p1 + p2 + p3);
            }

            assert(area > kEpsilon);
            c = (1.0f / area) * c + s;
            return c;
        }

    } 

    void Polygon::Set(const Math::Vec2 *points, int32_t pointCount)
    {
        assert(3 <= pointCount && pointCount <= kMaxPolygonVertices);
        if (pointCount < 3)
        {
            SetAsBox(1.0f, 1.0f);
            return;
        }

        int32_t n = pointCount < kMaxPolygonVertices ? pointCount : kMaxPolygonVertices;

        Math::Vec2 ps[kMaxPolygonVertices];
        int32_t tempCount = 0;
        for (int32_t i = 0; i < n; ++i)
        {
            Math::Vec2 v = points[i];

            bool unique = true;
            for (int32_t j = 0; j < tempCount; ++j)
            {
                if (DistanceSquared(v, ps[j]) < (0.5f * kLinearSlop) * (0.5f * kLinearSlop))
                {
                    unique = false;
                    break;
                }
            }
            if (unique)
                ps[tempCount++] = v;
        }

        n = tempCount;
        if (n < 3)
        {
            SetAsBox(1.0f, 1.0f);
            return;
        }

        int32_t i0 = 0;
        float x0 = ps[0].x;
        for (int32_t i = 1; i < n; ++i)
        {
            float x = ps[i].x;
            if (x > x0 || (x == x0 && ps[i].y < ps[i0].y))
            {
                i0 = i;
                x0 = x;
            }
        }

        int32_t hull[kMaxPolygonVertices];
        int32_t m = 0;
        int32_t ih = i0;

        for (;;)
        {
            assert(m < kMaxPolygonVertices);
            hull[m] = ih;

            int32_t ie = 0;
            for (int32_t j = 1; j < n; ++j)
            {
                if (ie == ih)
                {
                    ie = j;
                    continue;
                }

                Math::Vec2 r = ps[ie] - ps[hull[m]];
                Math::Vec2 v = ps[j] - ps[hull[m]];
                float c = Cross(r, v);
                if (c < 0.0f)
                    ie = j;
                if (c == 0.0f && (v.x * v.x + v.y * v.y) > (r.x * r.x + r.y * r.y))
                    ie = j;
            }

            ++m;
            ih = ie;
            if (ie == i0)
                break;
        }

        if (m < 3)
        {
            SetAsBox(1.0f, 1.0f);
            return;
        }

        count = m;
        for (int32_t i = 0; i < m; ++i)
            vertices[i] = ps[hull[i]];

        for (int32_t i = 0; i < m; ++i)
        {
            int32_t i1 = i;
            int32_t i2 = i + 1 < m ? i + 1 : 0;
            Math::Vec2 edge = vertices[i2] - vertices[i1];
            assert((edge.x * edge.x + edge.y * edge.y) > kEpsilon * kEpsilon);
            normals[i] = Normalize(Cross(edge, 1.0f));
        }

        centroid = ComputePolygonCentroid(vertices, m);
    }

    AABB Polygon::ComputeAABB(const Transform &xf) const
    {
        Math::Vec2 lower = xf.Transform(vertices[0]);
        Math::Vec2 upper = lower;

        for (int32_t i = 1; i < count; ++i)
        {
            Math::Vec2 v = xf.Transform(vertices[i]);
            lower = Min(lower, v);
            upper = Max(upper, v);
        }

        Math::Vec2 r(radius, radius);
        AABB aabb;
        aabb.lowerBound = lower - r;
        aabb.upperBound = upper + r;
        return aabb;
    }

    MassData Polygon::ComputeMass(float density) const
    {
        assert(count >= 3);

        Math::Vec2 center(0.0f, 0.0f);
        float area = 0.0f;
        float I = 0.0f;

        Math::Vec2 s = vertices[0];
        const float inv3 = 1.0f / 3.0f;

        for (int32_t i = 0; i < count; ++i)
        {
            Math::Vec2 e1 = vertices[i] - s;
            Math::Vec2 e2 = i + 1 < count ? vertices[i + 1] - s : vertices[0] - s;

            float D = Cross(e1, e2);
            float triangleArea = 0.5f * D;
            area += triangleArea;
            center += triangleArea * inv3 * (e1 + e2);

            float ex1 = e1.x, ey1 = e1.y;
            float ex2 = e2.x, ey2 = e2.y;
            float intx2 = ex1 * ex1 + ex2 * ex1 + ex2 * ex2;
            float inty2 = ey1 * ey1 + ey2 * ey1 + ey2 * ey2;

            I += (0.25f * inv3 * D) * (intx2 + inty2);
        }

        MassData massData;
        massData.mass = density * area;

        assert(area > kEpsilon);
        center *= 1.0f / area;
        massData.center = center + s;

        massData.I = density * I;
        massData.I += massData.mass * (Dot(massData.center, massData.center) - Dot(center, center));

        return massData;
    }

} 