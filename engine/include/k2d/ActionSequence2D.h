#pragma once

#include "k2d/Color.h"
#include "k2d/Component.h"
#include "k2d/Easing.h"
#include <ct/vector.hpp>
#include <mathc.h>

namespace k2d
{
enum class ActionKind : unsigned char
{
    Color,
    Move,
    Scale,
    Turn,
    Pause
};

enum class ActionSequenceLoop : unsigned char
{
    OneShot,
    Loop
};

struct ActionStep
{
    ActionKind kind = ActionKind::Pause;
    // Target position (Move) or target scale (Scale); unused otherwise.
    Math::Vec2 vector{0.0f};
    // Target rotation in degrees (Turn); unused otherwise.
    float angleDegrees = 0.0f;
    // Target tint (Color); unused otherwise.
    Color color = Color::White();
    // Pause: how long to wait. Every other kind: time to reach the target.
    float duration = 0.5f;
    MotionEase ease = MotionEase::Linear;
};

// A "pulse"-style behaviour: a list of typed steps played one after another,
// each easing the owner (or its sprite tint) from wherever it stood when the
// step began to the step's target, over its own duration. Unlike
// MotionTween2D's tracks, steps never run in parallel.
class ActionSequence2D final : public Component
{
  public:
    static const ComponentType Type = ComponentType::ActionSequence;
    ActionSequence2D();

    void clearSteps();
    void addStep(const ActionStep& step);
    bool removeStep(std::size_t index);
    std::size_t stepCount() const
    {
        return mSteps.size();
    }
    ActionStep* stepAt(std::size_t index)
    {
        return index < mSteps.size() ? &mSteps[index] : nullptr;
    }
    const ActionStep* stepAt(std::size_t index) const
    {
        return index < mSteps.size() ? &mSteps[index] : nullptr;
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
    int currentStep() const
    {
        return mPlaying || mPaused ? static_cast<int>(mIndex) : -1;
    }
    void setLoop(ActionSequenceLoop value)
    {
        mLoop = value;
    }
    ActionSequenceLoop loop() const
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

  protected:
    void onAwake() override;
    void onUpdate(float deltaTime) override;

  private:
    void beginStep(std::size_t index);
    void apply(float easedT);
    ct::Vector<ActionStep> mSteps;
    std::size_t mIndex = 0;
    float mTime = 0.0f;
    Math::Vec2 mFromVector{0.0f};
    float mFromAngle = 0.0f;
    Color mFromColor;
    bool mPlaying = false, mPaused = false, mAutoplay = false;
    ActionSequenceLoop mLoop = ActionSequenceLoop::OneShot;
};
} // namespace k2d
