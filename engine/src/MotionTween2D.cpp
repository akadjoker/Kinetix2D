#include "k2d/MotionTween2D.h"
#include "k2d/GameObject.h"
#include <algorithm>
#include <cmath>
namespace k2d
{
MotionTween2D::MotionTween2D() : Component(Type, ComponentEventUpdate)
{
}
float MotionTween2D::Ease(float t, MotionEase e)
{
    t = std::max(0.0f, std::min(1.0f, t));
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
void MotionTween2D::clearTracks()
{
    mTracks.clear();
    stop();
}
void MotionTween2D::addTrack(const MotionTweenTrack& track)
{
    mTracks.push_back(track);
}
float MotionTween2D::duration() const
{
    float result = 0;
    for (const MotionTweenTrack& t : mTracks)
        if (t.enabled)
            result = std::max(result, t.delay + std::max(0.0f, t.duration));
    return result;
}
void MotionTween2D::play(bool restart)
{
    if (restart)
    {
        mTime = mForward ? 0.0f : duration();
    }
    mPlaying = !mTracks.empty();
    mPaused = false;
    apply();
}
void MotionTween2D::stop()
{
    mPlaying = false;
    mPaused = false;
}
void MotionTween2D::onAwake()
{
    if (mAutoplay)
        play();
}
void MotionTween2D::apply()
{
    if (!owner())
        return;
    for (const MotionTweenTrack& t : mTracks)
    {
        if (!t.enabled)
            continue;
        float local = t.duration <= 0 ? 1 : (mTime - t.delay) / t.duration;
        float a = Ease(local, t.ease);
        Math::Vec2 v = t.from + (t.to - t.from) * a;
        if (t.property == MotionTweenProperty::Position)
            owner()->setPosition(v);
        else if (t.property == MotionTweenProperty::Scale)
            owner()->setScale(v);
        else
            owner()->setRotationDegrees(v.x);
    }
}
void MotionTween2D::onUpdate(float dt)
{
    if (!mPlaying || mPaused)
        return;
    float end = duration();
    mTime += (mForward ? dt : -dt);
    bool finished = false;
    if (mTime >= end || mTime <= 0)
    {
        if (mLoop == MotionTweenLoop::None)
        {
            mTime = mForward ? end : 0;
            mPlaying = false;
            finished = true;
        }
        else if (mLoop == MotionTweenLoop::Repeat)
            mTime = mForward ? 0 : end;
        else
        {
            mTime = mForward ? end : 0;
            mForward = !mForward;
        }
    }
    apply();
    if (finished && mOneShot && owner())
        owner()->removeComponent(this);
}
} // namespace k2d
