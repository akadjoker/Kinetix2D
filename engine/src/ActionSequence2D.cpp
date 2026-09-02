#include "k2d/ActionSequence2D.h"
#include "k2d/Utils.h"
#include "k2d/GameObject.h"
#include "k2d/Scene.h"
#include "k2d/SpriteComponent.h"

namespace k2d
{
ActionSequence2D::ActionSequence2D() : Component(Type, ComponentEventUpdate)
{
}
void ActionSequence2D::clearSteps()
{
    mSteps.clear();
    stop();
    mIndex = 0;
    mTime = 0.0f;
}
void ActionSequence2D::addStep(const ActionStep& step)
{
    mSteps.push_back(step);
}
void ActionSequence2D::addParallelStep()
{
    ActionStep step;
    step.kind = ActionKind::Parallel;
    mSteps.push_back(step);
}
ActionData* ActionSequence2D::addParallelAction(std::size_t stepIndex, const ActionData& action)
{
    ActionStep* step = stepAt(stepIndex);
    if (!step || step->kind != ActionKind::Parallel || action.kind == ActionKind::Parallel)
        return nullptr;
    step->actions.push_back(action);
    return &step->actions.back();
}
bool ActionSequence2D::removeParallelAction(std::size_t stepIndex, std::size_t actionIndex)
{
    ActionStep* step = stepAt(stepIndex);
    if (!step || step->kind != ActionKind::Parallel || actionIndex >= step->actions.size())
        return false;
    step->actions.erase(step->actions.begin() + actionIndex);
    return true;
}
bool ActionSequence2D::removeStep(std::size_t index)
{
    if (index >= mSteps.size())
        return false;
    mSteps.erase(mSteps.begin() + index);
    return true;
}
void ActionSequence2D::beginStep(std::size_t index)
{
    mIndex = index;
    mTime = 0.0f;
    mParallelStates.clear();
    if (index >= mSteps.size())
        return;
    const ActionStep& step = mSteps[index];
    if (step.kind == ActionKind::Parallel)
    {
        for (std::size_t i = 0; i < step.actions.size(); ++i)
        {
            mParallelStates.push_back(ActionState{});
            beginAction(step.actions[i], mParallelStates.back());
        }
        return;
    }
    beginAction(step, mActionState);
}
void ActionSequence2D::beginAction(const ActionData& action, ActionState& state)
{
    if (action.kind == ActionKind::Event)
    {
        dispatchEvent(action.event);
        return;
    }
    if (!owner())
        return;
    switch (action.kind)
    {
    case ActionKind::Move:
        state.fromVector = owner()->position();
        break;
    case ActionKind::Scale:
        state.fromVector = owner()->scale();
        break;
    case ActionKind::Turn:
        state.fromAngle = owner()->rotationDegrees();
        break;
    case ActionKind::Color:
        if (SpriteComponent* sprite = owner()->getComponent<SpriteComponent>())
            state.fromColor = sprite->material().color();
        break;
    case ActionKind::Pause:
    case ActionKind::Parallel:
    case ActionKind::Event:
        break;
    }
}
void ActionSequence2D::applyAction(const ActionData& action, const ActionState& state, float easedT)
{
    if (!owner())
        return;
    switch (action.kind)
    {
    case ActionKind::Move:
        owner()->setPosition(state.fromVector + (action.vector - state.fromVector) * easedT);
        break;
    case ActionKind::Scale:
        owner()->setScale(state.fromVector + (action.vector - state.fromVector) * easedT);
        break;
    case ActionKind::Turn:
        owner()->setRotationDegrees(state.fromAngle + (action.angleDegrees - state.fromAngle) * easedT);
        break;
    case ActionKind::Color:
        if (SpriteComponent* sprite = owner()->getComponent<SpriteComponent>())
        {
            Color c = Color::Lerp(state.fromColor, action.color, easedT);
            sprite->setColor(static_cast<unsigned char>(c.r * 255.0f + 0.5f), static_cast<unsigned char>(c.g * 255.0f + 0.5f),
                              static_cast<unsigned char>(c.b * 255.0f + 0.5f), static_cast<unsigned char>(c.a * 255.0f + 0.5f));
        }
        break;
    case ActionKind::Pause:
    case ActionKind::Parallel:
    case ActionKind::Event:
        break;
    }
}
float ActionSequence2D::durationFor(const ActionData& action) const
{
    return action.kind == ActionKind::Event ? 0.0f : Max(0.0f, action.duration);
}
void ActionSequence2D::dispatchEvent(const ct::String& event)
{
    if (!event.empty() && owner() && owner()->scene())
        owner()->scene()->dispatchActionEvent(owner(), event.c_str());
}
void ActionSequence2D::play(bool restart)
{
    if (mSteps.empty())
    {
        mPlaying = false;
        return;
    }
    if (restart || mIndex >= mSteps.size())
        beginStep(0);
    mPlaying = true;
    mPaused = false;
}
void ActionSequence2D::stop()
{
    mPlaying = false;
    mPaused = false;
}
void ActionSequence2D::onAwake()
{
    if (mAutoplay)
        play();
}
void ActionSequence2D::onStart()
{
    // Serialized components are configured after attachment, and an editor
    // preview may have already finished before Play starts. Re-arm autoplay
    // at the beginning of the simulation as well.
    if (mAutoplay)
        play(true);
}
void ActionSequence2D::onUpdate(float dt)
{
    if (!mPlaying || mPaused || mSteps.empty())
        return;
    mTime += dt;
    const ActionStep& step = mSteps[mIndex];
    float dur = step.kind == ActionKind::Parallel ? 0.0f : durationFor(step);
    if (step.kind == ActionKind::Parallel)
        for (std::size_t i = 0; i < step.actions.size(); ++i)
            dur = Max(dur, durationFor(step.actions[i]));
    float local = dur <= 0.0f ? 1.0f : Max(0.0f, Min(1.0f, mTime / dur));
    if (step.kind == ActionKind::Parallel)
    {
        for (std::size_t i = 0; i < step.actions.size(); ++i)
        {
            const ActionData& action = step.actions[i];
            const float actionDuration = durationFor(action);
            const float actionTime = actionDuration <= 0.0f ? 1.0f : Max(0.0f, Min(1.0f, mTime / actionDuration));
            applyAction(action, mParallelStates[i], Ease(actionTime, action.ease));
        }
    }
    else
    {
        applyAction(step, mActionState, Ease(local, step.ease));
    }
    if (mTime >= dur)
    {
        std::size_t next = mIndex + 1;
        if (next >= mSteps.size())
        {
            if (mLoop == ActionSequenceLoop::Loop)
                beginStep(0);
            else
                mPlaying = false;
        }
        else
        {
            beginStep(next);
        }
    }
}
} // namespace k2d
