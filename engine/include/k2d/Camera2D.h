#pragma once

#include "k2d/Matrix2D.h"

#include <mathc.h>

#include <cmath>
#include <cstdlib>

namespace k2d
{

struct Camera2D
{
    Math::Vec2 position;
    float rotationDegrees;
    Math::Vec2 zoom;
    Math::Vec2 offset;
    bool limitEnabled;
    Math::Vec4 limits;
    bool smoothingEnabled;
    float smoothingSpeed;
    bool deadZoneEnabled;
    Math::Vec4 deadZone;
    bool targetEnabled;
    Math::Vec2 target;

    // Runtime-only screen-space feedback. These values are deliberately
    // separate from `offset`: authored camera placement stays stable and
    // scene serialization never captures a shake in progress.
    struct ShakeState
    {
        bool active = false;
        float amplitudeX = 0.0f;
        float amplitudeY = 0.0f;
        float frequency = 0.0f;
        float cycles = 0.0f;
        float cyclesLeft = 0.0f;
    };

    struct TraumaState
    {
        float value = 0.0f;
        float amplitudeX = 0.0f;
        float amplitudeY = 0.0f;
        float frequency = 24.0f;
        float decay = 1.5f;
        float time = 0.0f;
    };

    struct ZoomPunchState
    {
        bool active = false;
        float amount = 0.0f;
        float duration = 0.0f;
        float elapsed = 0.0f;
        float scale = 1.0f;
    };

    Math::Vec2 shakeOffset;
    ShakeState shake;
    TraumaState trauma;
    ZoomPunchState zoomPunch;

    Camera2D()
        : position(0.0f), rotationDegrees(0.0f), zoom(1.0f, 1.0f), offset(0.0f), limitEnabled(false), limits(0.0f),
          smoothingEnabled(false), smoothingSpeed(5.0f), deadZoneEnabled(false), deadZone(0.0f), targetEnabled(false),
          target(0.0f), shakeOffset(0.0f), shake(), trauma(), zoomPunch()
    {
    }

    void setLimits(float left, float top, float right, float bottom)
    {
        limits = Math::Vec4(left, top, right, bottom);
        limitEnabled = right > left && bottom > top;
    }

    void clearLimits()
    {
        limitEnabled = false;
    }

    void setSmoothing(bool enabled, float speed = 5.0f)
    {
        smoothingEnabled = enabled;
        smoothingSpeed = speed > 0.0f ? speed : 0.0f;
    }

    void setDeadZonePixels(float left, float top, float right, float bottom)
    {
        deadZone = Math::Vec4(left, top, right, bottom);
        deadZoneEnabled = right > left && bottom > top;
    }

    void clearDeadZone()
    {
        deadZoneEnabled = false;
    }
    void setTarget(const Math::Vec2& value)
    {
        target = value;
        targetEnabled = true;
    }
    void clearTarget()
    {
        targetEnabled = false;
    }

    // `amplitudeX/Y` are pixels on screen. `durationCycles` follows the
    // original ZenPy View2D API: 8 Hz for 3 cycles lasts 0.375 seconds.
    void startShake(float amplitudeX, float amplitudeY, float frequency, float durationCycles)
    {
        if (frequency <= 0.0f || durationCycles <= 0.0f)
        {
            stopShake();
            return;
        }
        shake.active = true;
        shake.amplitudeX = amplitudeX;
        shake.amplitudeY = amplitudeY;
        shake.frequency = frequency;
        shake.cycles = durationCycles;
        shake.cyclesLeft = durationCycles;
    }

    void stopShake()
    {
        shake.active = false;
        shake.cyclesLeft = 0.0f;
    }

    void addTrauma(float amount)
    {
        if (amount <= 0.0f)
            return;
        trauma.value += amount;
        if (trauma.value > 1.0f)
            trauma.value = 1.0f;
    }

    void setTraumaProfile(float amplitudeX, float amplitudeY, float frequency, float decay)
    {
        trauma.amplitudeX = amplitudeX < 0.0f ? 0.0f : amplitudeX;
        trauma.amplitudeY = amplitudeY < 0.0f ? 0.0f : amplitudeY;
        trauma.frequency = frequency < 0.0f ? 0.0f : frequency;
        trauma.decay = decay < 0.0f ? 0.0f : decay;
    }

    void clearTrauma()
    {
        trauma.value = 0.0f;
        trauma.time = 0.0f;
    }

    bool isShaking() const
    {
        return shake.active || trauma.value > 0.0f;
    }
    float traumaValue() const
    {
        return trauma.value;
    }

    void startZoomPunch(float amount, float duration)
    {
        if (duration <= 0.0f)
        {
            stopZoomPunch();
            return;
        }
        zoomPunch.active = true;
        zoomPunch.amount = amount;
        zoomPunch.duration = duration;
        zoomPunch.elapsed = 0.0f;
        zoomPunch.scale = 1.0f;
    }

    void stopZoomPunch()
    {
        zoomPunch.active = false;
        zoomPunch.elapsed = 0.0f;
        zoomPunch.scale = 1.0f;
    }

    bool isZoomPunching() const
    {
        return zoomPunch.active;
    }

    void update(float deltaTime, float screenW, float screenH)
    {
        updateEffects(deltaTime);
        if (!targetEnabled)
        {
            clampToLimits(screenW, screenH);
            return;
        }

        Math::Vec2 desired = position;
        Math::Vec2 worldDeadZoneMin = target;
        Math::Vec2 worldDeadZoneMax = target;
        const Math::Vec2 currentZoom = effectiveZoom();
        if (deadZoneEnabled)
        {
            worldDeadZoneMin += Math::Vec2((deadZone.x - screenW * 0.5f) / currentZoom.x,
                                           (deadZone.y - screenH * 0.5f) / currentZoom.y);
            worldDeadZoneMax += Math::Vec2((deadZone.z - screenW * 0.5f) / currentZoom.x,
                                           (deadZone.w - screenH * 0.5f) / currentZoom.y);
        }
        else
        {
            worldDeadZoneMin = target;
            worldDeadZoneMax = target;
        }

        if (target.x < desired.x + worldDeadZoneMin.x - target.x)
            desired.x = target.x - (deadZoneEnabled ? (deadZone.x - screenW * 0.5f) / currentZoom.x : 0.0f);
        if (target.x > desired.x + worldDeadZoneMax.x - target.x)
            desired.x = target.x - (deadZoneEnabled ? (deadZone.z - screenW * 0.5f) / currentZoom.x : 0.0f);
        if (target.y < desired.y + worldDeadZoneMin.y - target.y)
            desired.y = target.y - (deadZoneEnabled ? (deadZone.y - screenH * 0.5f) / currentZoom.y : 0.0f);
        if (target.y > desired.y + worldDeadZoneMax.y - target.y)
            desired.y = target.y - (deadZoneEnabled ? (deadZone.w - screenH * 0.5f) / currentZoom.y : 0.0f);

        if (!smoothingEnabled || smoothingSpeed <= 0.0f || deltaTime <= 0.0f)
            position = desired;
        else
        {
            float factor = 1.0f - expf(-smoothingSpeed * deltaTime);
            position += (desired - position) * factor;
        }

        clampToLimits(screenW, screenH);
    }

    void clampToLimits(float screenW, float screenH)
    {
        if (!limitEnabled)
            return;
        const Math::Vec2 currentZoom = effectiveZoom();
        float halfW = screenW * 0.5f / currentZoom.x;
        float halfH = screenH * 0.5f / currentZoom.y;
        float minCenterX = limits.x + halfW;
        float maxCenterX = limits.z - halfW;
        float minCenterY = limits.y + halfH;
        float maxCenterY = limits.w - halfH;
        position.x = minCenterX <= maxCenterX
                         ? (position.x < minCenterX ? minCenterX : (position.x > maxCenterX ? maxCenterX : position.x))
                         : (limits.x + limits.z) * 0.5f;
        position.y = minCenterY <= maxCenterY
                         ? (position.y < minCenterY ? minCenterY : (position.y > maxCenterY ? maxCenterY : position.y))
                         : (limits.y + limits.w) * 0.5f;
    }

    Matrix2D CameraXform(float screenW, float screenH) const
    {
        const Math::Vec2 currentZoom = effectiveZoom();
        Math::Vec2 zoomScale(1.0f / currentZoom.x, 1.0f / currentZoom.y);
        float angle = rotationDegrees * 0.01745329251f;
        Math::Vec2 screenOffset = Math::Vec2(screenW, screenH) * 0.5f * zoomScale;
        Math::Vec2 rotated(screenOffset.x * cosf(angle) - screenOffset.y * sinf(angle),
                           screenOffset.x * sinf(angle) + screenOffset.y * cosf(angle));
        Math::Vec2 origin = -rotated + position + offset;
        return Matrix2D::Translation(origin.x, origin.y) * Matrix2D::Rotation(rotationDegrees) *
               Matrix2D::Scaling(zoomScale.x, zoomScale.y) * Matrix2D::Translation(-shakeOffset.x, -shakeOffset.y);
    }

    Matrix2D ViewXform(float screenW, float screenH) const
    {
        const Math::Vec2 currentZoom = effectiveZoom();
        Math::Vec2 zoomScale(1.0f / currentZoom.x, 1.0f / currentZoom.y);
        float angle = rotationDegrees * 0.01745329251f;
        Math::Vec2 screenOffset = Math::Vec2(screenW, screenH) * 0.5f * zoomScale;
        Math::Vec2 rotated(screenOffset.x * cosf(angle) - screenOffset.y * sinf(angle),
                           screenOffset.x * sinf(angle) + screenOffset.y * cosf(angle));
        Math::Vec2 origin = -rotated + position + offset;
        return Matrix2D::Translation(shakeOffset.x, shakeOffset.y) * Matrix2D::Scaling(currentZoom.x, currentZoom.y) *
               Matrix2D::Rotation(-rotationDegrees) * Matrix2D::Translation(-origin.x, -origin.y);
    }

    Math::Mat4 Projection(float screenW, float screenH) const
    {
        Math::Mat4 ndc = Math::Mat4::Ortho(0.0f, screenW, screenH, 0.0f, -1.0f, 1.0f);
        return ndc * ViewXform(screenW, screenH).ToMat4();
    }

    Math::Vec2 ScreenToWorld(float sx, float sy, float screenW, float screenH) const
    {
        return CameraXform(screenW, screenH).Transform(sx, sy);
    }

    void VisibleRect(float& minX, float& minY, float& maxX, float& maxY, float screenW, float screenH) const
    {
        Math::Vec2 corners[4] = {
            ScreenToWorld(0.0f, 0.0f, screenW, screenH), ScreenToWorld(screenW, 0.0f, screenW, screenH),
            ScreenToWorld(screenW, screenH, screenW, screenH), ScreenToWorld(0.0f, screenH, screenW, screenH)};
        minX = maxX = corners[0].x;
        minY = maxY = corners[0].y;
        for (int i = 1; i < 4; ++i)
        {
            if (corners[i].x < minX)
                minX = corners[i].x;
            if (corners[i].x > maxX)
                maxX = corners[i].x;
            if (corners[i].y < minY)
                minY = corners[i].y;
            if (corners[i].y > maxY)
                maxY = corners[i].y;
        }
    }

  private:
    Math::Vec2 effectiveZoom() const
    {
        const float scale = zoomPunch.scale > 0.001f ? zoomPunch.scale : 0.001f;
        const float x = zoom.x * scale;
        const float y = zoom.y * scale;
        return Math::Vec2(x > 0.001f ? x : 0.001f, y > 0.001f ? y : 0.001f);
    }

    void updateEffects(float deltaTime)
    {
        shakeOffset = Math::Vec2(0.0f);
        if (deltaTime <= 0.0f)
            return;

        if (shake.active)
        {
            shake.cyclesLeft -= deltaTime * shake.frequency;
            if (shake.cyclesLeft > 0.0f && shake.cycles > 0.0f)
            {
                const float fraction = shake.cyclesLeft / shake.cycles;
                const float elapsedCycles = shake.cycles - shake.cyclesLeft;
                const float wave = fraction * fraction * cosf(elapsedCycles * 6.28318530718f);
                // Matches the old View2D's alternating impulse shake.
                const float signX = (std::rand() & 1) == 0 ? -1.0f : 1.0f;
                const float signY = (std::rand() & 1) == 0 ? -1.0f : 1.0f;
                shakeOffset += Math::Vec2(shake.amplitudeX * signX * wave, shake.amplitudeY * signY * wave);
            }
            else
            {
                stopShake();
            }
        }

        if (trauma.value > 0.0f)
        {
            trauma.time += deltaTime;
            const float strength = trauma.value * trauma.value;
            const float phase = trauma.time * trauma.frequency * 6.28318530718f;
            shakeOffset += Math::Vec2(sinf(phase) * trauma.amplitudeX * strength,
                                      cosf(phase + 2.32477856366f) * trauma.amplitudeY * strength);
            trauma.value -= trauma.decay * deltaTime;
            if (trauma.value < 0.0f)
                trauma.value = 0.0f;
        }

        if (zoomPunch.active)
        {
            zoomPunch.elapsed += deltaTime;
            const float t = zoomPunch.elapsed / zoomPunch.duration;
            if (t >= 1.0f)
            {
                stopZoomPunch();
            }
            else
            {
                zoomPunch.scale = 1.0f + zoomPunch.amount * sinf(t * 3.14159265359f);
            }
        }
    }
};

} // namespace k2d
