#pragma once

namespace k2d
{
enum class MotionEase : unsigned char
{
    Linear,
    InQuad,
    OutQuad,
    InOutQuad,
    InCubic,
    OutCubic,
    InOutCubic,
    InSine,
    OutSine,
    InOutSine,
    InBack,
    OutBack,
    InOutBack,
    InBounce,
    OutBounce,
    InOutBounce,
    InElastic,
    OutElastic,
    InOutElastic
};

float Ease(float value, MotionEase ease);
} // namespace k2d
