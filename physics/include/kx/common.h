#pragma once

#include <glm/glm.hpp>
#include <cmath>
#include <cstdint>

#include <k2d/Matrix2D.h>

namespace kx
{

    using Transform = k2d::Matrix2D;

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

    inline float Cross(const glm::vec2 &a, const glm::vec2 &b) { return a.x * b.y - a.y * b.x; }
    inline glm::vec2 Cross(const glm::vec2 &a, float s) { return glm::vec2(s * a.y, -s * a.x); }
    inline glm::vec2 Cross(float s, const glm::vec2 &a) { return glm::vec2(-s * a.y, s * a.x); }
    inline float Dot(const glm::vec2 &a, const glm::vec2 &b) { return a.x * b.x + a.y * b.y; }
    inline float DistanceSquared(const glm::vec2 &a, const glm::vec2 &b)
    {
        glm::vec2 c = a - b;
        return Dot(c, c);
    }
    inline float Distance(const glm::vec2 &a, const glm::vec2 &b) { return std::sqrt(DistanceSquared(a, b)); }

    inline glm::vec2 Normalize(const glm::vec2 &v)
    {
        float length = std::sqrt(v.x * v.x + v.y * v.y);
        if (length < kEpsilon)
            return glm::vec2(0.0f, 0.0f);
        float invLength = 1.0f / length;
        return glm::vec2(v.x * invLength, v.y * invLength);
    }

    inline Transform MakeTransform(const glm::vec2 &pos, float angleRad)
    {
        float c = std::cos(angleRad);
        float s = std::sin(angleRad);
        return Transform(c, s, -s, c, pos.x, pos.y);
    }

    inline glm::vec2 Rotate(const Transform &xf, const glm::vec2 &v)
    {
        return glm::vec2(xf.a * v.x + xf.c * v.y, xf.b * v.x + xf.d * v.y);
    }

    inline glm::vec2 InvRotate(const Transform &xf, const glm::vec2 &v)
    {
        return glm::vec2(xf.a * v.x + xf.b * v.y, xf.c * v.x + xf.d * v.y);
    }

    inline glm::vec2 InvTransformPoint(const Transform &xf, const glm::vec2 &p)
    {
        float px = p.x - xf.tx;
        float py = p.y - xf.ty;
        return glm::vec2(xf.a * px + xf.b * py, xf.c * px + xf.d * py);
    }

} 