#include "k2d/DirectionalLight2D.h"

#include "k2d/GameObject.h"
#include "k2d/RenderQueue.h"

#include <cmath>

namespace k2d
{

    DirectionalLight2D::DirectionalLight2D()
        : Component(Type, ComponentEventRender), mColor(1.0f), mEnergy(1.0f),
          mCastShadow(false), mShadowColor(0.0f, 0.0f, 0.0f, 1.0f),
          mShadowFilter(SHADOW_FILTER_NEAREST), mCullMask(1), mHeight(0.0f)
    {
    }

    void DirectionalLight2D::setCullMask(unsigned int mask)
    {
        mCullMask = mask;
    }

    void DirectionalLight2D::setColor(float r, float g, float b, float a)
    {
        mColor = glm::vec4(r, g, b, a);
    }

    void DirectionalLight2D::setEnergy(float energy)
    {
        mEnergy = energy;
    }

    void DirectionalLight2D::setCastShadow(bool castShadow)
    {
        mCastShadow = castShadow;
    }

    void DirectionalLight2D::setShadowColor(float r, float g, float b, float a)
    {
        mShadowColor = glm::vec4(r, g, b, a);
    }

    void DirectionalLight2D::setShadowFilter(ShadowFilter filter)
    {
        mShadowFilter = filter;
    }

    void DirectionalLight2D::onRender(RenderQueue &queue)
    {
        Matrix2D transform = owner()->globalTransform();
        glm::vec2 direction = transform.Transform(0.0f, 1.0f) - transform.Position();
        float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
        if (length <= 0.0001f)
            return;

        DirectionalLight light;
        light.direction = -direction / length;
        light.color = mColor;
        light.color.a *= mEnergy;
        light.useShadow = mCastShadow;
        light.shadowFilter = mShadowFilter;
        light.shadowColor = mShadowColor;
        light.cullMask = mCullMask;
        light.height = mHeight;
        queue.AddDirectionalLight(light);
    }

} 