#pragma once

#include "../body.h"
#include "../shapes.h"

namespace kx
{

    struct ShapeRayCastOutput
    {
        glm::vec2 normal; 
        float fraction;   
    };

    bool RayCastCircle(const glm::vec2 &origin, const glm::vec2 &translation, float maxFraction,
                       const Circle &circle, const Transform &xf, ShapeRayCastOutput &out);

    bool RayCastPolygon(const glm::vec2 &origin, const glm::vec2 &translation, float maxFraction,
                        const Polygon &polygon, const Transform &xf, ShapeRayCastOutput &out);

    bool RayCastEdge(const glm::vec2 &origin, const glm::vec2 &translation, float maxFraction,
                     const Edge &edge, const Transform &xf, ShapeRayCastOutput &out);

    bool RayCastShape(const glm::vec2 &origin, const glm::vec2 &translation, float maxFraction,
                      const Shape &shape, const Transform &xf, ShapeRayCastOutput &out);

} 