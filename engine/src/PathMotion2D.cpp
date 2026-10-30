#include "k2d/PathMotion2D.h"
#include "k2d/Utils.h"
#include "k2d/GameObject.h"

namespace k2d
{
PathMotion2D::PathMotion2D() : Component(Type, ComponentEventUpdate)
{
}
void PathMotion2D::clearKeyframes()
{
    mKeyframes.clear();
    stop();
}
void PathMotion2D::addKeyframe(const PathMotionKeyframe& keyframe)
{
    mKeyframes.push_back(keyframe);
}
bool PathMotion2D::removeKeyframe(std::size_t index)
{
    if (index >= mKeyframes.size())
        return false;
    mKeyframes.erase(mKeyframes.begin() + index);
    return true;
}
float PathMotion2D::totalDuration() const
{
    const std::size_t n = mKeyframes.size();
    if (n < 2)
        return 0.0f;
    const std::size_t segments = (mLoop == PathMotionLoop::Repeat) ? n : n - 1;
    float total = 0.0f;
    for (std::size_t i = 0; i < segments; ++i)
        total += Max(0.0f, mKeyframes[i].duration);
    return total;
}
void PathMotion2D::play(bool restart)
{
    if (restart)
        mTime = mForward ? 0.0f : totalDuration();
    mPlaying = !mKeyframes.empty();
    mPaused = false;
    apply();
}
void PathMotion2D::stop()
{
    mPlaying = false;
    mPaused = false;
}
void PathMotion2D::onAwake()
{
    if (mAutoplay)
        play();
}
void PathMotion2D::apply()
{
    if (!owner())
        return;
    const std::size_t n = mKeyframes.size();
    if (n == 0)
        return;
    if (n == 1)
    {
        const PathMotionKeyframe& kf = mKeyframes[0];
        owner()->setPosition(kf.position);
        owner()->setScale(kf.scale);
        owner()->setRotationDegrees(kf.angleDegrees);
        return;
    }
    const std::size_t segments = (mLoop == PathMotionLoop::Repeat) ? n : n - 1;
    float t = mTime;
    std::size_t index = 0;
    float segmentDuration = 0.0f;
    for (; index < segments; ++index)
    {
        segmentDuration = Max(0.0f, mKeyframes[index].duration);
        if (t <= segmentDuration || index + 1 == segments)
            break;
        t -= segmentDuration;
    }
    const PathMotionKeyframe& from = mKeyframes[index];
    const PathMotionKeyframe& to = mKeyframes[(index + 1) % n];
    float local = segmentDuration <= 0.0f ? 1.0f : Max(0.0f, Min(1.0f, t / segmentDuration));
    float a = Ease(local, from.ease);
    owner()->setPosition(from.position + (to.position - from.position) * a);
    owner()->setScale(from.scale + (to.scale - from.scale) * a);
    owner()->setRotationDegrees(from.angleDegrees + (to.angleDegrees - from.angleDegrees) * a);
}
void PathMotion2D::onUpdate(float dt)
{
    if (!mPlaying || mPaused)
        return;
    float end = totalDuration();
    mTime += (mForward ? dt : -dt);
    bool finished = false;
    if (mTime >= end || mTime <= 0.0f)
    {
        if (mLoop == PathMotionLoop::None)
        {
            mTime = mForward ? end : 0.0f;
            mPlaying = false;
            finished = true;
        }
        else if (mLoop == PathMotionLoop::Repeat)
            mTime = mForward ? 0.0f : end;
        else
        {
            mTime = mForward ? end : 0.0f;
            mForward = !mForward;
        }
    }
    apply();
    if (finished && mOneShot && owner())
        owner()->removeComponent(this);
}
} // namespace k2d
