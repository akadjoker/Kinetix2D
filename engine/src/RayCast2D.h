#pragma once

#include "k2d/Geometry2D.h"

namespace k2d
{

struct ShapeRayCastOutput
{
    Math::Vec2 normal;
    float fraction;
};

bool RayCastCircle(const Math::Vec2 &origin, const Math::Vec2 &translation, float maxFraction,
                   const Circle &circle, const Transform &xf, ShapeRayCastOutput &out);

bool RayCastPolygon(const Math::Vec2 &origin, const Math::Vec2 &translation, float maxFraction,
                    const Polygon &polygon, const Transform &xf, ShapeRayCastOutput &out);

bool RayCastEdge(const Math::Vec2 &origin, const Math::Vec2 &translation, float maxFraction,
                 const Edge &edge, const Transform &xf, ShapeRayCastOutput &out);

bool RayCastShape(const Math::Vec2 &origin, const Math::Vec2 &translation, float maxFraction,
                  const Shape &shape, const Transform &xf, ShapeRayCastOutput &out);

}
