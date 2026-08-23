#pragma once

#include "k2d/Component.h"
#include "k2d/ParticleSystem.h"

namespace k2d
{

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