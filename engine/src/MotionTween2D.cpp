#include "k2d/MotionTween2D.h"
#include "k2d/Utils.h"
#include "k2d/GameObject.h"
#include <cmath>
namespace k2d
{
MotionTween2D::MotionTween2D() : Component(Type, ComponentEventUpdate)
{
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
            result = Max(result, t.delay + Max(0.0f, t.duration));
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
