#include "kx/manifold.h"

namespace kx
{

    void WorldManifold::Initialize(const Manifold *manifold,
                                   const Transform &xfA, float radiusA,
                                   const Transform &xfB, float radiusB)
    {
        if (manifold->pointCount == 0)
            return;

        switch (manifold->type)
        {
        case Manifold::kCircles:
        {
            normal = glm::vec2(1.0f, 0.0f);
            glm::vec2 pointA = xfA.Transform(manifold->localPoint);
            glm::vec2 pointB = xfB.Transform(manifold->points[0].localPoint);
            if (DistanceSquared(pointA, pointB) > kEpsilon * kEpsilon)
                normal = Normalize(pointB - pointA);

            glm::vec2 cA = pointA + radiusA * normal;
            glm::vec2 cB = pointB - radiusB * normal;
            points[0] = 0.5f * (cA + cB);
            separations[0] = Dot(cB - cA, normal);
            break;
        }

        case Manifold::kFaceA:
        {
            normal = Rotate(xfA, manifold->localNormal);
            glm::vec2 planePoint = xfA.Transform(manifold->localPoint);

            for (int32_t i = 0; i < manifold->pointCount; ++i)
            {
                glm::vec2 clipPoint = xfB.Transform(manifold->points[i].localPoint);
                glm::vec2 cA = clipPoint + (radiusA - Dot(clipPoint - planePoint, normal)) * normal;
                glm::vec2 cB = clipPoint - radiusB * normal;
                points[i] = 0.5f * (cA + cB);
                separations[i] = Dot(cB - cA, normal);
            }
            break;
        }

        case Manifold::kFaceB:
        {
            normal = Rotate(xfB, manifold->localNormal);
            glm::vec2 planePoint = xfB.Transform(manifold->localPoint);

            for (int32_t i = 0; i < manifold->pointCount; ++i)
            {
                glm::vec2 clipPoint = xfA.Transform(manifold->points[i].localPoint);
                glm::vec2 cB = clipPoint + (radiusB - Dot(clipPoint - planePoint, normal)) * normal;
                glm::vec2 cA = clipPoint - radiusA * normal;
                points[i] = 0.5f * (cA + cB);
                separations[i] = Dot(cA - cB, normal);
            }

            normal = -normal;
            break;
        }
        }
    }

} // namespace kx
