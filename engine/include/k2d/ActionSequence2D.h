#pragma once

#include "k2d/Color.h"
#include "k2d/Component.h"
#include "k2d/Easing.h"
#include <ct/string.hpp>
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
    Pause,
    // Changes only the Sprite alpha, preserving its RGB tint.
    Fade,
    // Runs its child actions at the same time. Parallel actions cannot be
    // nested; the sequence itself remains the ordering mechanism.
    Parallel,
    // Dispatches its named event to scripts on the owner, then continues.
    Event
};

enum class ActionSequenceLoop : unsigned char
{
    OneShot,
    Loop
};

// A leaf action. Parallel groups contain these values, so the data remains
// deliberately one level deep and cannot form recursive action trees.
struct ActionData
{
    ActionKind kind = ActionKind::Pause;
    // Target position (Move) or target scale (Scale); unused otherwise.
    Math::Vec2 vector{0.0f};
    // Target rotation in degrees (Turn); unused otherwise.
    float angleDegrees = 0.0f;
    // Target tint (Color); unused otherwise.
    Color color = Color::White();
    // Target opacity in the 0..1 range (Fade); unused otherwise.
    float alpha = 1.0f;
    // Pause: how long to wait. Every other kind: time to reach the target.
    float duration = 0.5f;
    MotionEase ease = MotionEase::Linear;
    // Event: name delivered to the owner. Parallel: actions started together.
    ct::String event;
};

struct ActionStep : public ActionData
{
    ct::Vector<ActionData> actions;
};

// A "pulse"-style behaviour: a list of typed steps played one after another,
// each easing the owner (or its sprite tint) from wherever it stood when the
// step began to the step's target, over its own duration. A Parallel step is
// the explicit exception: its child actions run together.
class ActionSequence2D final : public Component
{
  public:
    static const ComponentType Type = ComponentType::ActionSequence;
    ActionSequence2D();

    void clearSteps();
    void addStep(const ActionStep& step);
    void addParallelStep();
    ActionData* addParallelAction(std::size_t stepIndex, const ActionData& action);
    bool removeParallelAction(std::size_t stepIndex, std::size_t actionIndex);
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

    // For the editor's edit-mode viewport preview, the same way
    // Animation2D::Advance() is used there -- not part of the normal
    // Component update path.
    void Advance(float deltaTime)
    {
        onUpdate(deltaTime);
    }
    void restart()
    {
        if (!mSteps.empty())
            beginStep(0);
    }

  protected:
    void onAwake() override;
    void onStart() override;
    void onUpdate(float deltaTime) override;

  private:
    struct ActionState
    {
        Math::Vec2 fromVector{0.0f};
        float fromAngle = 0.0f;
        Color fromColor;
    };

    void beginStep(std::size_t index);
    void beginAction(const ActionData& action, ActionState& state);
    void applyAction(const ActionData& action, const ActionState& state, float easedT);
    float durationFor(const ActionData& action) const;
    void dispatchEvent(const ct::String& event);
    ct::Vector<ActionStep> mSteps;
    std::size_t mIndex = 0;
    float mTime = 0.0f;
    ActionState mActionState;
    ct::Vector<ActionState> mParallelStates;
    bool mPlaying = false, mPaused = false, mAutoplay = false;
    ActionSequenceLoop mLoop = ActionSequenceLoop::OneShot;
};
} // namespace k2d
