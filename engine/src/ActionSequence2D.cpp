#include "k2d/ActionSequence2D.h"
#include "k2d/Utils.h"
#include "k2d/GameObject.h"
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
    if (!owner() || index >= mSteps.size())
        return;
    switch (mSteps[index].kind)
    {
    case ActionKind::Move:
        mFromVector = owner()->position();
        break;
    case ActionKind::Scale:
        mFromVector = owner()->scale();
        break;
    case ActionKind::Turn:
        mFromAngle = owner()->rotationDegrees();
        break;
    case ActionKind::Color:
        if (SpriteComponent* sprite = owner()->getComponent<SpriteComponent>())
            mFromColor = sprite->material().color();
        break;
    case ActionKind::Pause:
        break;
    }
}
void ActionSequence2D::apply(float easedT)
{
    if (!owner() || mIndex >= mSteps.size())
        return;
    const ActionStep& step = mSteps[mIndex];
    switch (step.kind)
    {
    case ActionKind::Move:
        owner()->setPosition(mFromVector + (step.vector - mFromVector) * easedT);
        break;
    case ActionKind::Scale:
        owner()->setScale(mFromVector + (step.vector - mFromVector) * easedT);
        break;
    case ActionKind::Turn:
        owner()->setRotationDegrees(mFromAngle + (step.angleDegrees - mFromAngle) * easedT);
        break;
    case ActionKind::Color:
        if (SpriteComponent* sprite = owner()->getComponent<SpriteComponent>())
        {
            Color c = Color::Lerp(mFromColor, step.color, easedT);
            sprite->setColor(static_cast<unsigned char>(c.r * 255.0f + 0.5f), static_cast<unsigned char>(c.g * 255.0f + 0.5f),
                              static_cast<unsigned char>(c.b * 255.0f + 0.5f), static_cast<unsigned char>(c.a * 255.0f + 0.5f));
        }
        break;
    case ActionKind::Pause:
        break;
    }
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
void ActionSequence2D::onUpdate(float dt)
{
    if (!mPlaying || mPaused || mSteps.empty())
        return;
    mTime += dt;
    const ActionStep& step = mSteps[mIndex];
    float dur = Max(0.0f, step.duration);
    float local = dur <= 0.0f ? 1.0f : Max(0.0f, Min(1.0f, mTime / dur));
    apply(Ease(local, step.ease));
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
