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
            normal = Math::Vec2(1.0f, 0.0f);
            Math::Vec2 pointA = xfA.Transform(manifold->localPoint);
            Math::Vec2 pointB = xfB.Transform(manifold->points[0].localPoint);
            if (DistanceSquared(pointA, pointB) > kEpsilon * kEpsilon)
                normal = Normalize(pointB - pointA);

            Math::Vec2 cA = pointA + radiusA * normal;
            Math::Vec2 cB = pointB - radiusB * normal;
            points[0] = 0.5f * (cA + cB);
            separations[0] = Dot(cB - cA, normal);
            break;
        }

        case Manifold::kFaceA:
        {
            normal = Rotate(xfA, manifold->localNormal);
            Math::Vec2 planePoint = xfA.Transform(manifold->localPoint);

            for (int32_t i = 0; i < manifold->pointCount; ++i)
            {
                Math::Vec2 clipPoint = xfB.Transform(manifold->points[i].localPoint);
                Math::Vec2 cA = clipPoint + (radiusA - Dot(clipPoint - planePoint, normal)) * normal;
                Math::Vec2 cB = clipPoint - radiusB * normal;
                points[i] = 0.5f * (cA + cB);
                separations[i] = Dot(cB - cA, normal);
            }
            break;
        }

        case Manifold::kFaceB:
        {
            normal = Rotate(xfB, manifold->localNormal);
            Math::Vec2 planePoint = xfB.Transform(manifold->localPoint);

            for (int32_t i = 0; i < manifold->pointCount; ++i)
            {
                Math::Vec2 clipPoint = xfA.Transform(manifold->points[i].localPoint);
                Math::Vec2 cB = clipPoint + (radiusB - Dot(clipPoint - planePoint, normal)) * normal;
                Math::Vec2 cA = clipPoint - radiusA * normal;
                points[i] = 0.5f * (cA + cB);
                separations[i] = Dot(cA - cB, normal);
            }

            normal = -normal;
            break;
        }
        }
    }

} 