#include "k2d/ParticleComponent.h"

#include "k2d/GameObject.h"
#include "k2d/RenderQueue.h"

namespace k2d
{

    ParticleComponent::ParticleComponent(size_t capacity)
        : Component(Type, ComponentEventUpdate | ComponentEventRender),
          mSystem(capacity), mBlendMode(BLEND_MIX), mYSort(false), mFollowOwner(true)
    {
    }

    void ParticleComponent::onUpdate(float deltaTime)
    {
        if (mFollowOwner)
            mSystem.SetEmitterPosition(owner()->globalPosition());
        mSystem.Update(deltaTime);
    }

    void ParticleComponent::onRender(RenderQueue &queue)
    {
        mSystem.Submit(queue, owner()->zIndex(), mYSort, mBlendMode);
    }

}