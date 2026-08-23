#pragma once

#include "k2d/Component.h"
#include "k2d/ParticleSystem.h"

namespace k2d
{

    // Wraps a ParticleSystem as an ordinary attachable Component: give it a
    // GameObject, position it with setPosition like any other node, and the
    // Scene drives Update/Submit every frame automatically -- no more manual
    // fire.Update(dt); fire.Submit(queue, z); in caller code. ParticleSystem
    // itself is unchanged and still works standalone for callers that don't
    // want a GameObject.
    class ParticleComponent : public Component
    {
    public:
        static const ComponentType Type = ComponentType::Particle;

        explicit ParticleComponent(size_t capacity = 256);

        ParticleSystem &system() { return mSystem; }
        const ParticleSystem &system() const { return mSystem; }

        void setBlendMode(BlendMode mode) { mBlendMode = mode; }
        BlendMode blendMode() const { return mBlendMode; }
        void setYSort(bool ySort) { mYSort = ySort; }
        bool ySort() const { return mYSort; }

        // When true (default), the emitter position is synced to the owning
        // GameObject's world position every frame -- move the object, the
        // effect (and any particles it's about to emit) moves with it.
        // Turn off for a "fire and forget" effect that should stay put in
        // world space once started even if its owner moves on.
        void setFollowOwner(bool follow) { mFollowOwner = follow; }
        bool followOwner() const { return mFollowOwner; }

    protected:
        void onUpdate(float deltaTime) override;
        void onRender(RenderQueue &queue) override;

    private:
        ParticleSystem mSystem;
        BlendMode mBlendMode;
        bool mYSort;
        bool mFollowOwner;
    };

}
