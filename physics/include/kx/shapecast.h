#pragma once

#include "shapes.h"

namespace kx
{

struct Shape;

// Linear convex shape cast. This is a direct C++ adaptation of Box2D's
// GJK shape-cast (Erin Catto, MIT), used here for character motion.
// Shape B moves by translationB while shape A stays fixed.
struct ShapeCastInput
{
    const Shape* shapeA = nullptr;
    Transform transformA;
    const Shape* shapeB = nullptr;
    Transform transformB;
    Math::Vec2 translationB = Math::Vec2(0.0f, 0.0f);
};

struct ShapeCastOutput
{
    Math::Vec2 point = Math::Vec2(0.0f, 0.0f);
    Math::Vec2 normal = Math::Vec2(0.0f, 0.0f);
    float fraction = 1.0f;
    int iterations = 0;
};

// Returns false on a miss or when the shapes start overlapped. Rotations
// are fixed during the cast; rotational CCD remains the solver's TOI job.
bool ShapeCast(const ShapeCastInput& input, ShapeCastOutput& output);

} // namespace kx
