#include "k2d/ParallaxLayerComponent.h"

#include "k2d/GameObject.h"
#include "k2d/Scene.h"

namespace k2d
{

    ParallaxLayerComponent::ParallaxLayerComponent()
        : Component(Type, ComponentEventRender), mLayer()
    {
    }

    void ParallaxLayerComponent::onRender(RenderQueue &queue)
    {
        Scene *scene = owner()->scene();
        if (!scene)
            return;
        const Camera2D *camera = scene->renderCamera();
        if (!camera || scene->renderViewportWidth() <= 0.0f || scene->renderViewportHeight() <= 0.0f)
            return;
        mLayer.submit(queue, *camera, scene->renderViewportWidth(), scene->renderViewportHeight());
    }

}
