#pragma once

#include <mathc.h>
#include <cstdint>

#include "common.h"

namespace kx
{

    struct AABB
    {
        Math::Vec2 lowerBound;
        Math::Vec2 upperBound;

        Math::Vec2 GetCenter() const { return 0.5f * (lowerBound + upperBound); }
        Math::Vec2 GetExtents() const { return 0.5f * (upperBound - lowerBound); }

        float GetPerimeter() const
        {
            float wx = upperBound.x - lowerBound.x;
            float wy = upperBound.y - lowerBound.y;
            return 2.0f * (wx + wy);
        }

        void Combine(const AABB &aabb)
        {
            lowerBound = Min(lowerBound, aabb.lowerBound);
            upperBound = Max(upperBound, aabb.upperBound);
        }

        void Combine(const AABB &aabb1, const AABB &aabb2)
        {
            lowerBound = Min(aabb1.lowerBound, aabb2.lowerBound);
            upperBound = Max(aabb1.upperBound, aabb2.upperBound);
        }

        bool Contains(const AABB &aabb) const
        {
            return lowerBound.x <= aabb.lowerBound.x &&
                   lowerBound.y <= aabb.lowerBound.y &&
                   aabb.upperBound.x <= upperBound.x &&
                   aabb.upperBound.y <= upperBound.y;
        }
    };

    inline bool TestOverlap(const AABB &a, const AABB &b)
    {
        Math::Vec2 d1 = b.lowerBound - a.upperBound;
        Math::Vec2 d2 = a.lowerBound - b.upperBound;

        if (d1.x > 0.0f || d1.y > 0.0f)
            return false;
        if (d2.x > 0.0f || d2.y > 0.0f)
            return false;
        return true;
    }

    struct MassData
    {
        float mass;
        Math::Vec2 center;
        float I;
    };

    struct Circle
    {
        Math::Vec2 center = Math::Vec2(0.0f, 0.0f);
        float radius = 0.0f;

        AABB ComputeAABB(const Transform &xf) const;
        MassData ComputeMass(float density) const;
    };

    struct Edge
    {
        Edge() : radius(kPolygonRadius), oneSided(false)
        {
            vertex0 = Math::Vec2(0.0f, 0.0f);
            vertex3 = Math::Vec2(0.0f, 0.0f);
        }

        void SetTwoSided(const Math::Vec2 &v1, const Math::Vec2 &v2);

        AABB ComputeAABB(const Transform &xf) const;
        MassData ComputeMass(float density) const;

        Math::Vec2 vertex1, vertex2;
        Math::Vec2 vertex0, vertex3;
        float radius;
        bool oneSided;
    };

    constexpr int32_t kMaxPolygonVertices = 8;

    struct Polygon
    {
        Polygon() : count(0), radius(kPolygonRadius) { centroid = Math::Vec2(0.0f, 0.0f); }

        void Set(const Math::Vec2 *points, int32_t pointCount);
        void SetAsBox(float halfWidth, float halfHeight);
        void SetAsBox(float halfWidth, float halfHeight, const Math::Vec2 &center, float angle);

        AABB ComputeAABB(const Transform &xf) const;
        MassData ComputeMass(float density) const;

        Math::Vec2 centroid;
        Math::Vec2 vertices[kMaxPolygonVertices];
        Math::Vec2 normals[kMaxPolygonVertices];
        int32_t count;
        float radius;
    };

} 