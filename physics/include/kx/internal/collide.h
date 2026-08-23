#pragma once

#include "../manifold.h"
#include "../shapes.h"

namespace kx
{

    void CollideCircles(Manifold *manifold,
                        const Circle &circleA, const Transform &xfA,
                        const Circle &circleB, const Transform &xfB);

    void CollidePolygonAndCircle(Manifold *manifold,
                                 const Polygon &polygonA, const Transform &xfA,
                                 const Circle &circleB, const Transform &xfB);

    void CollidePolygons(Manifold *manifold,
                         const Polygon &polyA, const Transform &xfA,
                         const Polygon &polyB, const Transform &xfB);

    void CollideEdgeAndCircle(Manifold *manifold,
                              const Edge &edgeA, const Transform &xfA,
                              const Circle &circleB, const Transform &xfB);

    void CollideEdgeAndPolygon(Manifold *manifold,
                               const Edge &edgeA, const Transform &xfA,
                               const Polygon &polygonB, const Transform &xfB);

} 