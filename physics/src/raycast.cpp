#include "kx/internal/raycast.h"

#include <cmath>

namespace kx
{

    bool RayCastCircle(const glm::vec2 &origin, const glm::vec2 &translation, float maxFraction,
                       const Circle &circle, const Transform &xf, ShapeRayCastOutput &out)
    {
        // Trabalha no referencial local da shape (mesma tatica de CollidePolygonAndCircle):
        // resolve p1 + t*d = ponto no circulo, |ponto - centro| = raio, para t.
        glm::vec2 p1 = InvTransformPoint(xf, origin);
        glm::vec2 d = InvRotate(xf, translation);

        glm::vec2 s = p1 - circle.center;
        float b = Dot(s, s) - circle.radius * circle.radius;

        float rr = Dot(d, d);
        if (rr < kEpsilon)
            return false;

        float c = Dot(s, d);
        float sigma = c * c - rr * b;
        if (sigma < 0.0f)
            return false;

        float t = -(c + std::sqrt(sigma));
        if (t < 0.0f || t > maxFraction * rr)
            return false;

        t /= rr;
        glm::vec2 localPoint = p1 + t * d;
        out.fraction = t;
        out.normal = Rotate(xf, Normalize(localPoint - circle.center));
        return true;
    }

    bool RayCastPolygon(const glm::vec2 &origin, const glm::vec2 &translation, float maxFraction,
                        const Polygon &polygon, const Transform &xf, ShapeRayCastOutput &out)
    {
        // Clipping do segmento contra o meio-espaco de cada face (poligono convexo):
        // [lower, upper] encolhe a cada face ate sobrar (quando muito) o troco visivel
        // a partir de fora. Mesma tecnica do b2PolygonShape::RayCast.
        glm::vec2 p1 = InvTransformPoint(xf, origin);
        glm::vec2 p2 = InvTransformPoint(xf, origin + translation);
        glm::vec2 d = p2 - p1;

        float lower = 0.0f;
        float upper = maxFraction;
        int32_t index = -1;

        for (int32_t i = 0; i < polygon.count; ++i)
        {
            float numerator = Dot(polygon.normals[i], polygon.vertices[i] - p1);
            float denominator = Dot(polygon.normals[i], d);

            if (denominator == 0.0f)
            {
                if (numerator < 0.0f)
                    return false;
            }
            else
            {
                if (denominator < 0.0f && numerator < lower * denominator)
                {
                    lower = numerator / denominator;
                    index = i;
                }
                else if (denominator > 0.0f && numerator < upper * denominator)
                {
                    upper = numerator / denominator;
                }
            }

            if (upper < lower)
                return false;
        }

        if (index < 0)
            return false;

        out.fraction = lower;
        out.normal = Rotate(xf, polygon.normals[index]);
        return true;
    }

    bool RayCastEdge(const glm::vec2 &origin, const glm::vec2 &translation, float maxFraction,
                     const Edge &edge, const Transform &xf, ShapeRayCastOutput &out)
    {
        glm::vec2 p1 = InvTransformPoint(xf, origin);
        glm::vec2 d = InvRotate(xf, translation);

        glm::vec2 v1 = edge.vertex1;
        glm::vec2 v2 = edge.vertex2;
        glm::vec2 e = v2 - v1;

        float length = std::sqrt(Dot(e, e));
        if (length < kEpsilon)
            return false;

        glm::vec2 normal = glm::vec2(e.y, -e.x) * (1.0f / length);

        float denominator = Dot(d, normal);
        if (std::fabs(denominator) < kEpsilon)
            return false; // raio paralelo a edge

        float t = Dot(normal, v1 - p1) / denominator;
        if (t < 0.0f || t > maxFraction)
            return false;

        glm::vec2 point = p1 + t * d;
        float s = Dot(point - v1, e) / Dot(e, e);
        if (s < 0.0f || s > 1.0f)
            return false;

        out.fraction = t;
        out.normal = Rotate(xf, denominator > 0.0f ? -normal : normal);
        return true;
    }

    bool RayCastShape(const glm::vec2 &origin, const glm::vec2 &translation, float maxFraction,
                      const Shape &shape, const Transform &xf, ShapeRayCastOutput &out)
    {
        switch (shape.type)
        {
        case ShapeType::Circle:
            return RayCastCircle(origin, translation, maxFraction, shape.circle, xf, out);
        case ShapeType::Polygon:
            return RayCastPolygon(origin, translation, maxFraction, shape.polygon, xf, out);
        case ShapeType::Edge:
        default:
            return RayCastEdge(origin, translation, maxFraction, shape.edge, xf, out);
        }
    }

} // namespace kx
