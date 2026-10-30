#pragma once

#include <mathc.h>
#include <cstdint>

#include "k2d/Geometry2D.h"

namespace k2d
{

constexpr int32_t kMaxManifoldPoints = 2;

struct ContactFeature
{
    enum Type
    {
        kVertex = 0,
        kFace = 1
    };

    uint8_t indexA;
    uint8_t indexB;
    uint8_t typeA;
    uint8_t typeB;
};

union ContactID
{
    ContactFeature cf;
    uint32_t key;
};

struct ManifoldPoint
{
    Math::Vec2 localPoint;
    float normalImpulse;
    float tangentImpulse;
    ContactID id;
};

struct Manifold
{
    enum Type
    {
        kCircles,
        kFaceA,
        kFaceB
    };

    ManifoldPoint points[kMaxManifoldPoints];
    Math::Vec2 localNormal;
    Math::Vec2 localPoint;
    Type type;
    int32_t pointCount;
};

struct WorldManifold
{
    void Initialize(const Manifold *manifold,
                    const Transform &xfA, float radiusA,
                    const Transform &xfB, float radiusB);

    Math::Vec2 normal;
    Math::Vec2 points[kMaxManifoldPoints];
    float separations[kMaxManifoldPoints];
};

} // namespace k2d
