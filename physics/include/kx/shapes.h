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
    };

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

} // namespace kx
