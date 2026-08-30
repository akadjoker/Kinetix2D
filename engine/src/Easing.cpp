#include "k2d/Easing.h"
#include "k2d/Utils.h"
#include <cmath>
namespace k2d
{
float Ease(float t, MotionEase e)
{
    t = Max(0.0f, Min(1.0f, t));
    const float pi = 3.14159265f;
    const auto outBounce = [](float x)
    {
        const float n = 7.5625f, d = 2.75f;
        if (x < 1 / d)
            return n * x * x;
        if (x < 2 / d)
        {
            x -= 1.5f / d;
            return n * x * x + .75f;
        }
        if (x < 2.5f / d)
        {
            x -= 2.25f / d;
            return n * x * x + .9375f;
        }
        x -= 2.625f / d;
        return n * x * x + .984375f;
    };
    switch (e)
    {
    case MotionEase::InQuad:
        return t * t;
    case MotionEase::OutQuad:
        return t * (2 - t);
    case MotionEase::InOutQuad:
        return t < .5f ? 2 * t * t : -1 + (4 - 2 * t) * t;
    case MotionEase::InCubic:
        return t * t * t;
    case MotionEase::OutCubic:
    {
        float q = 1 - t;
        return 1 - q * q * q;
    }
    case MotionEase::InOutCubic:
        return t < .5f ? 4 * t * t * t : 1 - std::pow(-2 * t + 2, 3) / 2;
    case MotionEase::InSine:
        return 1 - std::cos(t * pi * .5f);
    case MotionEase::OutSine:
        return std::sin(t * pi * .5f);
    case MotionEase::InOutSine:
        return -(std::cos(pi * t) - 1) * .5f;
    case MotionEase::InBack:
        return 2.70158f * t * t * t - 1.70158f * t * t;
    case MotionEase::OutBack:
    {
        float q = t - 1;
        return 1 + 2.70158f * q * q * q + 1.70158f * q * q;
    }
    case MotionEase::InOutBack:
    {
        float c = 1.70158f * 1.525f;
        return t < .5f ? (std::pow(2 * t, 2) * ((c + 1) * 2 * t - c)) / 2
                       : (std::pow(2 * t - 2, 2) * ((c + 1) * (t * 2 - 2) + c) + 2) / 2;
    }
    case MotionEase::OutBounce:
        return outBounce(t);
    case MotionEase::InBounce:
        return 1 - outBounce(1 - t);
    case MotionEase::InOutBounce:
        return t < .5f ? (1 - outBounce(1 - 2 * t)) * .5f : (1 + outBounce(2 * t - 1)) * .5f;
    case MotionEase::InElastic:
        return t == 0 || t == 1 ? t : -std::pow(2, 10 * t - 10) * std::sin((t * 10 - 10.75f) * (2 * pi / 3));
    case MotionEase::OutElastic:
        return t == 0 || t == 1 ? t : std::pow(2, -10 * t) * std::sin((t * 10 - .75f) * (2 * pi / 3)) + 1;
    case MotionEase::InOutElastic:
        return t == 0 || t == 1
                   ? t
                   : (t < .5f ? -std::pow(2, 20 * t - 10) * std::sin((20 * t - 11.125f) * (2 * pi / 4.5f)) / 2
                              : std::pow(2, -20 * t + 10) * std::sin((20 * t - 11.125f) * (2 * pi / 4.5f)) / 2 + 1);
    default:
        return t;
    }
}
} // namespace k2d
