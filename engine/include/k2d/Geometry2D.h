#pragma once

#include <mathc.h>
#include <cmath>
#include <cstdint>

#include "k2d/Matrix2D.h"

namespace k2d
{

using Transform = Matrix2D;

constexpr float kEpsilon = 1.1920929e-7f;
constexpr float kPi = 3.14159265359f;
constexpr float kLinearSlop = 0.5f;
constexpr float kPolygonRadius = 2.0f * kLinearSlop;
constexpr float kSpeculativeDistance = 4.0f * kLinearSlop;
constexpr float kBaumgarte = 0.2f;
constexpr float kVelocityThreshold = 50.0f;
constexpr float kMaxLinearCorrection = 10.0f;
constexpr float kMaxTranslation = 100.0f;
constexpr float kAabbExtension = 10.0f;
constexpr float kAabbMultiplier = 4.0f;
constexpr float kAngularSlop = 2.0f / 180.0f * kPi;
constexpr float kMaxAngularCorrection = 8.0f / 180.0f * kPi;
constexpr float kSleepVelocity = 2.5f;
constexpr float kSleepAngularVelocity = 0.05f;
constexpr float kTimeToSleep = 0.5f;

inline float Clamp(float a, float lo, float hi) { return a < lo ? lo : (a > hi ? hi : a); }

inline float Cross(const Math::Vec2 &a, const Math::Vec2 &b) { return a.x * b.y - a.y * b.x; }
inline Math::Vec2 Cross(const Math::Vec2 &a, float s) { return Math::Vec2(s * a.y, -s * a.x); }
inline Math::Vec2 Cross(float s, const Math::Vec2 &a) { return Math::Vec2(-s * a.y, s * a.x); }
inline float Dot(const Math::Vec2 &a, const Math::Vec2 &b) { return a.x * b.x + a.y * b.y; }
inline float DistanceSquared(const Math::Vec2 &a, const Math::Vec2 &b)
{
    Math::Vec2 c = a - b;
    return Dot(c, c);
}
inline float Distance(const Math::Vec2 &a, const Math::Vec2 &b) { return std::sqrt(DistanceSquared(a, b)); }

inline Math::Vec2 Normalize(const Math::Vec2 &v)
{
    float length = std::sqrt(v.x * v.x + v.y * v.y);
    if (length < kEpsilon)
        return Math::Vec2(0.0f, 0.0f);
    float invLength = 1.0f / length;
    return Math::Vec2(v.x * invLength, v.y * invLength);
}

inline Transform MakeTransform(const Math::Vec2 &pos, float angleRad)
{
    float c = std::cos(angleRad);
    float s = std::sin(angleRad);
    return Transform(c, s, -s, c, pos.x, pos.y);
}

inline Math::Vec2 Rotate(const Transform &xf, const Math::Vec2 &v)
{
    return Math::Vec2(xf.a * v.x + xf.c * v.y, xf.b * v.x + xf.d * v.y);
}

inline Math::Vec2 InvRotate(const Transform &xf, const Math::Vec2 &v)
{
    return Math::Vec2(xf.a * v.x + xf.b * v.y, xf.c * v.x + xf.d * v.y);
}

inline Math::Vec2 InvTransformPoint(const Transform &xf, const Math::Vec2 &p)
{
    float px = p.x - xf.tx;
    float py = p.y - xf.ty;
    return Math::Vec2(xf.a * px + xf.b * py, xf.c * px + xf.d * py);
}

inline Math::Vec2 Min(const Math::Vec2 &a, const Math::Vec2 &b) { return Math::Vec2::Min(a, b); }
inline Math::Vec2 Max(const Math::Vec2 &a, const Math::Vec2 &b) { return Math::Vec2::Max(a, b); }

struct AABB
{
    Math::Vec2 lowerBound;
    Math::Vec2 upperBound;

    Math::Vec2 GetCenter() const { return 0.5f * (lowerBound + upperBound); }
    Math::Vec2 GetExtents() const { return 0.5f * (upperBound - lowerBound); }

    float GetPerimeter() const
    {
        float wx = upperBound.x - lowerBound.x;
        float wy = upperBound.y - lowerBound.y;
        return 2.0f * (wx + wy);
    }

    void Combine(const AABB &aabb)
    {
        lowerBound = Min(lowerBound, aabb.lowerBound);
        upperBound = Max(upperBound, aabb.upperBound);
    }

    void Combine(const AABB &aabb1, const AABB &aabb2)
    {
        lowerBound = Min(aabb1.lowerBound, aabb2.lowerBound);
        upperBound = Max(aabb1.upperBound, aabb2.upperBound);
    }

    bool Contains(const AABB &aabb) const
    {
        return lowerBound.x <= aabb.lowerBound.x &&
               lowerBound.y <= aabb.lowerBound.y &&
               aabb.upperBound.x <= upperBound.x &&
               aabb.upperBound.y <= upperBound.y;
    }
};

inline bool TestOverlap(const AABB &a, const AABB &b)
{
    Math::Vec2 d1 = b.lowerBound - a.upperBound;
    Math::Vec2 d2 = a.lowerBound - b.upperBound;

    if (d1.x > 0.0f || d1.y > 0.0f)
        return false;
    if (d2.x > 0.0f || d2.y > 0.0f)
        return false;
    return true;
}

struct MassData
{
    float mass;
    Math::Vec2 center;
    float I;
};

struct Circle
{
    Math::Vec2 center = Math::Vec2(0.0f, 0.0f);
    float radius = 0.0f;

    AABB ComputeAABB(const Transform &xf) const;
    MassData ComputeMass(float density) const;
};

struct Edge
{
    Edge() : radius(kPolygonRadius), oneSided(false)
    {
        vertex0 = Math::Vec2(0.0f, 0.0f);
        vertex3 = Math::Vec2(0.0f, 0.0f);
    }

    void SetTwoSided(const Math::Vec2 &v1, const Math::Vec2 &v2);

    AABB ComputeAABB(const Transform &xf) const;
    MassData ComputeMass(float density) const;

    Math::Vec2 vertex1, vertex2;
    Math::Vec2 vertex0, vertex3;
    float radius;
    bool oneSided;
};

constexpr int32_t kMaxPolygonVertices = 8;

struct Polygon
{
    Polygon() : count(0), radius(kPolygonRadius) { centroid = Math::Vec2(0.0f, 0.0f); }

    void Set(const Math::Vec2 *points, int32_t pointCount);
    void SetAsBox(float halfWidth, float halfHeight);
    void SetAsBox(float halfWidth, float halfHeight, const Math::Vec2 &center, float angle);

    AABB ComputeAABB(const Transform &xf) const;
    MassData ComputeMass(float density) const;

    Math::Vec2 centroid;
    Math::Vec2 vertices[kMaxPolygonVertices];
    Math::Vec2 normals[kMaxPolygonVertices];
    int32_t count;
    float radius;
};

enum class BodyType : unsigned char
{
    Static,
    Kinematic,
    Dynamic
};

enum class ShapeType : unsigned char
{
    Circle,
    Polygon,
    Edge
};

struct Filter
{
    uint16_t category;
    uint16_t mask;
    int16_t group;

    Filter() : category(1), mask(0xFFFF), group(0)
    {
    }
};

inline bool ShouldCollide(const Filter &a, const Filter &b)
{
    if (a.group == b.group && a.group != 0)
        return a.group > 0;
    return (a.mask & b.category) != 0 && (b.mask & a.category) != 0;
}

struct Shape
{
    ShapeType type;
    float density;
    Filter filter;
    bool isSensor;
    void *userData;
    Circle circle;
    Polygon polygon;
    Edge edge;
};

inline float ShapeRadius(const Shape &shape)
{
    switch (shape.type)
    {
    case ShapeType::Circle:
        return shape.circle.radius;
    case ShapeType::Polygon:
        return shape.polygon.radius;
    default:
        return shape.edge.radius;
    }
}

} // namespace k2d
