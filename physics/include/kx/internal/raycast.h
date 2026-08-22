#pragma once

#include "../body.h"
#include "../shapes.h"

namespace kx
{

    struct ShapeRayCastOutput
    {
        glm::vec2 normal; // world-space, aponta para fora da shape
        float fraction;   // 0 na origem, 1 em origin+translation
    };

    // Todas recebem o raio em espaco do mundo (origin/translation) e a shape na sua
    // transform local — mesma convencao de (shape, xf) usada em ComputeAABB/ComputeMass.
    // maxFraction permite encolher a procura (usado por World::RayCast* para so aceitar
    // hits mais perto do que o melhor already encontrado).
    bool RayCastCircle(const glm::vec2 &origin, const glm::vec2 &translation, float maxFraction,
                       const Circle &circle, const Transform &xf, ShapeRayCastOutput &out);

    bool RayCastPolygon(const glm::vec2 &origin, const glm::vec2 &translation, float maxFraction,
                        const Polygon &polygon, const Transform &xf, ShapeRayCastOutput &out);

    // Trata a edge como um segmento fino (ignora o raio kPolygonRadius, tal como o
    // b2EdgeShape::RayCast do Box2D); os vertices "fantasma" (vertex0/vertex3) usados
    // para one-sided collision nao afetam o raycast.
    bool RayCastEdge(const glm::vec2 &origin, const glm::vec2 &translation, float maxFraction,
                     const Edge &edge, const Transform &xf, ShapeRayCastOutput &out);

    bool RayCastShape(const glm::vec2 &origin, const glm::vec2 &translation, float maxFraction,
                      const Shape &shape, const Transform &xf, ShapeRayCastOutput &out);

} // namespace kx
