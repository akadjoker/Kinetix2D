#include "k2d/Light2D.h"

#include "k2d/GameObject.h"
#include "k2d/RenderQueue.h"

namespace k2d
{

    Light2D::Light2D()
        : Component(Type, ComponentEventRender), mColor(1.0f, 1.0f, 1.0f, 1.0f),
          mEnergy(1.0f), mRadius(300.0f), mCastShadow(false),
          mShadowColor(0.0f, 0.0f, 0.0f, 1.0f), mShadowFilter(SHADOW_FILTER_NEAREST)
    {
    }

    void Light2D::setColor(float r, float g, float b, float a)
    {
        mColor = glm::vec4(r, g, b, a);
    }

    void Light2D::setEnergy(float energy)
    {
        mEnergy = energy;
    }

    void Light2D::setRadius(float radius)
    {
        mRadius = radius;
    }

    void Light2D::setCastShadow(bool castShadow)
    {
        mCastShadow = castShadow;
    }

    void Light2D::setShadowColor(float r, float g, float b, float a)
    {
        mShadowColor = glm::vec4(r, g, b, a);
    }

    void Light2D::setShadowFilter(ShadowFilter filter)
    {
        mShadowFilter = filter;
    }

    void Light2D::onRender(RenderQueue &queue)
    {
        PointLight light;
        light.position = owner()->globalPosition();
        light.color = mColor;
        light.color.a *= mEnergy;
        light.radius = mRadius;
        light.useShadow = mCastShadow;
        light.shadowFilter = mShadowFilter;
        light.shadowMatrix = owner()->globalTransform().AffineInverse();
        light.shadowColor = mShadowColor;
        queue.AddLight(light);
    }

} // namespace k2d
