#pragma once

#include <glm/glm.hpp>

#include "common.h"

namespace kx
{

    class World;

    struct Color
    {
        unsigned char r, g, b, a;
    };

    enum DebugDrawFlags : unsigned
    {
        DebugDrawShapes = 1 << 0,
        DebugDrawAABBs = 1 << 1,
        DebugDrawContacts = 1 << 2
    };

    class DebugDraw
    {
    public:
        virtual ~DebugDraw() {}

        virtual void DrawCircleShape(const Transform &xf, float radius, Color color) = 0;
        virtual void DrawPolygonShape(const Transform &xf, const glm::vec2 *verts, int count, Color color) = 0;
        virtual void DrawSegment(const glm::vec2 &a, const glm::vec2 &b, Color color) = 0;
        virtual void DrawPoint(const glm::vec2 &p, float size, Color color) = 0;
        virtual void DrawAABB(const glm::vec2 &lower, const glm::vec2 &upper, Color color) = 0;
    };

    void Draw(World &world, DebugDraw &draw, unsigned flags);

} // namespace kx
