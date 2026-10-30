#pragma once

#include "k2d/Component.h"
#include "k2d/Easing.h"
#include <ct/vector.hpp>
#include <mathc.h>

namespace k2d
{
enum class MotionTweenProperty : unsigned char
{
    Position,
    Rotation,
    Scale
};
enum class MotionTweenLoop : unsigned char
{
    None,
    Repeat,
    PingPong
};

struct MotionTweenTrack
{
    MotionTweenProperty property = MotionTweenProperty::Position;
    Math::Vec2 from{0.0f};
    Math::Vec2 to{0.0f};
    float duration = 0.5f;
    float delay = 0.0f;
    MotionEase ease = MotionEase::OutQuad;
    bool enabled = true;
};

class MotionTween2D final : public Component
{
  public:
    static const ComponentType Type = ComponentType::MotionTween;
    MotionTween2D();
    static float Ease(float value, MotionEase ease)
    {
        return k2d::Ease(value, ease);
    }
    void clearTracks();
    void addTrack(const MotionTweenTrack& track);
    std::size_t trackCount() const
    {
        return mTracks.size();
    }
    MotionTweenTrack* trackAt(std::size_t index)
    {
        return index < mTracks.size() ? &mTracks[index] : nullptr;
    }
    const MotionTweenTrack* trackAt(std::size_t index) const
    {
        return index < mTracks.size() ? &mTracks[index] : nullptr;
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
    void setLoop(MotionTweenLoop value)
    {
        mLoop = value;
    }
    MotionTweenLoop loop() const
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
    // One-shot tweens remove themselves after their final value is applied.
    // Disable this for an authored tween that will be triggered again later.
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
    float duration() const;
    ct::Vector<MotionTweenTrack> mTracks;
    float mTime = 0.0f;
    bool mPlaying = false, mPaused = false, mForward = true, mAutoplay = false, mOneShot = true;
    MotionTweenLoop mLoop = MotionTweenLoop::None;
};
} // namespace k2d
