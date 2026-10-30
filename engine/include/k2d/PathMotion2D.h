#pragma once

#include "k2d/Component.h"
#include "k2d/Easing.h"
#include <ct/vector.hpp>
#include <mathc.h>

namespace k2d
{
enum class PathMotionLoop : unsigned char
{
    None,
    Repeat,
    PingPong
};

struct PathMotionKeyframe
{
    Math::Vec2 position{0.0f};
    Math::Vec2 scale{1.0f};
    float angleDegrees = 0.0f;
    // Time to travel FROM this keyframe TO the next one. On the last
    // keyframe it only matters for the wrap-around segment back to
    // keyframe 0 when looping.
    float duration = 0.5f;
    MotionEase ease = MotionEase::Linear;
};

// An ordered pose path: a list of keyframes the owner is moved through in
// order, each segment eased with the ease of the keyframe it leaves from.
// A ship's custom flight paths are a list of these, swapped in by name and
// triggered with play().
class PathMotion2D final : public Component
{
  public:
    static const ComponentType Type = ComponentType::PathMotion;
    PathMotion2D();

    void clearKeyframes();
    void addKeyframe(const PathMotionKeyframe& keyframe);
    bool removeKeyframe(std::size_t index);
    std::size_t keyframeCount() const
    {
        return mKeyframes.size();
    }
    PathMotionKeyframe* keyframeAt(std::size_t index)
    {
        return index < mKeyframes.size() ? &mKeyframes[index] : nullptr;
    }
    const PathMotionKeyframe* keyframeAt(std::size_t index) const
    {
        return index < mKeyframes.size() ? &mKeyframes[index] : nullptr;
    }

    void play(bool restart = true);
    void stop();
    void pause(bool value = true)
    {
        mPaused = value;
    }
    bool playing() const
    {
        return mPlaying && !mPaused;
    }
    bool paused() const
    {
        return mPaused;
    }
    float time() const
    {
        return mTime;
    }
    void setLoop(PathMotionLoop value)
    {
        mLoop = value;
    }
    PathMotionLoop loop() const
    {
        return mLoop;
    }
    void setAutoplay(bool value)
    {
        mAutoplay = value;
    }
    bool autoplay() const
    {
        return mAutoplay;
    }
    // Unlike MotionTween2D, a path is meant to be replayed (a ship's flight
    // path should not vanish after its first play), so this defaults to
    // false. Set it for an authored one-off entrance/exit move.
    void setOneShot(bool value)
    {
        mOneShot = value;
    }
    bool oneShot() const
    {
        return mOneShot;
    }

  protected:
    void onAwake() override;
    void onUpdate(float deltaTime) override;

  private:
    void apply();
    float totalDuration() const;
    ct::Vector<PathMotionKeyframe> mKeyframes;
    float mTime = 0.0f;
    bool mPlaying = false, mPaused = false, mForward = true, mAutoplay = false, mOneShot = false;
    PathMotionLoop mLoop = PathMotionLoop::None;
};
} // namespace k2d
