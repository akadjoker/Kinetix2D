#pragma once

#include <mathc.h>

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
        DebugDrawContacts = 1 << 2,
        DebugDrawJoints = 1 << 3
    };

    class DebugDraw
    {
    public:
        virtual ~DebugDraw() {}

        virtual void DrawCircleShape(const Transform &xf, float radius, Color color) = 0;
        virtual void DrawPolygonShape(const Transform &xf, const Math::Vec2 *verts, int count, Color color) = 0;
        virtual void DrawSegment(const Math::Vec2 &a, const Math::Vec2 &b, Color color) = 0;
        virtual void DrawPoint(const Math::Vec2 &p, float size, Color color) = 0;
        virtual void DrawAABB(const Math::Vec2 &lower, const Math::Vec2 &upper, Color color) = 0;
    };

    void Draw(World &world, DebugDraw &draw, unsigned flags);

} 