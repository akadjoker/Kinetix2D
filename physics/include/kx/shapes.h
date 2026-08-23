#pragma once

#include <glm/glm.hpp>
#include <cstdint>

#include "common.h"

namespace kx
{

    struct AABB
    {
        glm::vec2 lowerBound;
        glm::vec2 upperBound;

        glm::vec2 GetCenter() const { return 0.5f * (lowerBound + upperBound); }
        glm::vec2 GetExtents() const { return 0.5f * (upperBound - lowerBound); }

        float GetPerimeter() const
        {
            float wx = upperBound.x - lowerBound.x;
            float wy = upperBound.y - lowerBound.y;
            return 2.0f * (wx + wy);
        }

        void Combine(const AABB &aabb)
        {
            lowerBound = glm::min(lowerBound, aabb.lowerBound);
            upperBound = glm::max(upperBound, aabb.upperBound);
        }

        void Combine(const AABB &aabb1, const AABB &aabb2)
        {
            lowerBound = glm::min(aabb1.lowerBound, aabb2.lowerBound);
            upperBound = glm::max(aabb1.upperBound, aabb2.upperBound);
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
        glm::vec2 d1 = b.lowerBound - a.upperBound;
        glm::vec2 d2 = a.lowerBound - b.upperBound;

        if (d1.x > 0.0f || d1.y > 0.0f)
            return false;
        if (d2.x > 0.0f || d2.y > 0.0f)
            return false;
        return true;
    }

    struct MassData
    {
        float mass;
        glm::vec2 center;
        float I;
    };

    struct Circle
    {
        glm::vec2 center = glm::vec2(0.0f, 0.0f);
        float radius = 0.0f;

        AABB ComputeAABB(const Transform &xf) const;
        MassData ComputeMass(float density) const;
    };

    struct Edge
    {
        Edge() : radius(kPolygonRadius), oneSided(false)
        {
            vertex0 = glm::vec2(0.0f, 0.0f);
            vertex3 = glm::vec2(0.0f, 0.0f);
        }

        void SetTwoSided(const glm::vec2 &v1, const glm::vec2 &v2);

        AABB ComputeAABB(const Transform &xf) const;
        MassData ComputeMass(float density) const;

        glm::vec2 vertex1, vertex2;
        glm::vec2 vertex0, vertex3;
        float radius;
        bool oneSided;
    };

    constexpr int32_t kMaxPolygonVertices = 8;

    struct Polygon
    {
        Polygon() : count(0), radius(kPolygonRadius) { centroid = glm::vec2(0.0f, 0.0f); }

        void Set(const glm::vec2 *points, int32_t pointCount);
        void SetAsBox(float halfWidth, float halfHeight);
        void SetAsBox(float halfWidth, float halfHeight, const glm::vec2 &center, float angle);

        AABB ComputeAABB(const Transform &xf) const;
        MassData ComputeMass(float density) const;

        glm::vec2 centroid;
        glm::vec2 vertices[kMaxPolygonVertices];
        glm::vec2 normals[kMaxPolygonVertices];
        int32_t count;
        float radius;
    };

} 