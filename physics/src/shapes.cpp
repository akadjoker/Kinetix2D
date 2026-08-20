#include "kx/shapes.h"

#include <cassert>
#include <cmath>

namespace kx
{

    AABB Circle::ComputeAABB(const Transform &xf) const
    {
        glm::vec2 p = xf.Transform(center);
        AABB aabb;
        aabb.lowerBound = glm::vec2(p.x - radius, p.y - radius);
        aabb.upperBound = glm::vec2(p.x + radius, p.y + radius);
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

    void Edge::SetTwoSided(const glm::vec2 &v1, const glm::vec2 &v2)
    {
        vertex1 = v1;
        vertex2 = v2;
        oneSided = false;
    }

    AABB Edge::ComputeAABB(const Transform &xf) const
    {
        glm::vec2 v1 = xf.Transform(vertex1);
        glm::vec2 v2 = xf.Transform(vertex2);

        glm::vec2 lower = glm::min(v1, v2);
        glm::vec2 upper = glm::max(v1, v2);

        glm::vec2 r(radius, radius);
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
        vertices[0] = glm::vec2(-halfWidth, -halfHeight);
        vertices[1] = glm::vec2(halfWidth, -halfHeight);
        vertices[2] = glm::vec2(halfWidth, halfHeight);
        vertices[3] = glm::vec2(-halfWidth, halfHeight);
        normals[0] = glm::vec2(0.0f, -1.0f);
        normals[1] = glm::vec2(1.0f, 0.0f);
        normals[2] = glm::vec2(0.0f, 1.0f);
        normals[3] = glm::vec2(-1.0f, 0.0f);
        centroid = glm::vec2(0.0f, 0.0f);
    }

    void Polygon::SetAsBox(float halfWidth, float halfHeight, const glm::vec2 &boxCenter, float angle)
    {
        SetAsBox(halfWidth, halfHeight);
        centroid = boxCenter;

        Transform xf = MakeTransform(boxCenter, angle);

        for (int32_t i = 0; i < count; ++i)
        {
            vertices[i] = xf.Transform(vertices[i]);
            normals[i] = glm::vec2(xf.a * normals[i].x + xf.c * normals[i].y,
                                    xf.b * normals[i].x + xf.d * normals[i].y);
        }
    }

    namespace
    {

        glm::vec2 ComputePolygonCentroid(const glm::vec2 *vs, int32_t vertCount)
        {
            assert(vertCount >= 3);

            glm::vec2 c(0.0f, 0.0f);
            float area = 0.0f;

            glm::vec2 s = vs[0];
            const float inv3 = 1.0f / 3.0f;

            for (int32_t i = 0; i < vertCount; ++i)
            {
                glm::vec2 p1 = vs[0] - s;
                glm::vec2 p2 = vs[i] - s;
                glm::vec2 p3 = i + 1 < vertCount ? vs[i + 1] - s : vs[0] - s;

                glm::vec2 e1 = p2 - p1;
                glm::vec2 e2 = p3 - p1;

                float triangleArea = 0.5f * Cross(e1, e2);
                area += triangleArea;
                c += triangleArea * inv3 * (p1 + p2 + p3);
            }

            assert(area > kEpsilon);
            c = (1.0f / area) * c + s;
            return c;
        }

    } // namespace

    void Polygon::Set(const glm::vec2 *points, int32_t pointCount)
    {
        assert(3 <= pointCount && pointCount <= kMaxPolygonVertices);
        if (pointCount < 3)
        {
            SetAsBox(1.0f, 1.0f);
            return;
        }

        int32_t n = pointCount < kMaxPolygonVertices ? pointCount : kMaxPolygonVertices;

        glm::vec2 ps[kMaxPolygonVertices];
        int32_t tempCount = 0;
        for (int32_t i = 0; i < n; ++i)
        {
            glm::vec2 v = points[i];

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

                glm::vec2 r = ps[ie] - ps[hull[m]];
                glm::vec2 v = ps[j] - ps[hull[m]];
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
            glm::vec2 edge = vertices[i2] - vertices[i1];
            assert((edge.x * edge.x + edge.y * edge.y) > kEpsilon * kEpsilon);
            normals[i] = Normalize(Cross(edge, 1.0f));
        }

        centroid = ComputePolygonCentroid(vertices, m);
    }

    AABB Polygon::ComputeAABB(const Transform &xf) const
    {
        glm::vec2 lower = xf.Transform(vertices[0]);
        glm::vec2 upper = lower;

        for (int32_t i = 1; i < count; ++i)
        {
            glm::vec2 v = xf.Transform(vertices[i]);
            lower = glm::min(lower, v);
            upper = glm::max(upper, v);
        }

        glm::vec2 r(radius, radius);
        AABB aabb;
        aabb.lowerBound = lower - r;
        aabb.upperBound = upper + r;
        return aabb;
    }

    MassData Polygon::ComputeMass(float density) const
    {
        assert(count >= 3);

        glm::vec2 center(0.0f, 0.0f);
        float area = 0.0f;
        float I = 0.0f;

        glm::vec2 s = vertices[0];
        const float inv3 = 1.0f / 3.0f;

        for (int32_t i = 0; i < count; ++i)
        {
            glm::vec2 e1 = vertices[i] - s;
            glm::vec2 e2 = i + 1 < count ? vertices[i + 1] - s : vertices[0] - s;

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

} // namespace kx
