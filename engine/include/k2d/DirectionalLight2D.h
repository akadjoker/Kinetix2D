#pragma once

#include "k2d/Component.h"
#include "k2d/CanvasTypes.h"

#include <glm/glm.hpp>

namespace k2d
{

    class DirectionalLight2D : public Component
    {
    public:
        static const ComponentType Type = ComponentType::Light;

        DirectionalLight2D();

        void setColor(float r, float g, float b, float a = 1.0f);
        const glm::vec4 &color() const { return mColor; }
        void setEnergy(float energy);
        float energy() const { return mEnergy; }
        void setCastShadow(bool castShadow);
        bool castShadow() const { return mCastShadow; }
        void setShadowColor(float r, float g, float b, float a = 1.0f);
        const glm::vec4 &shadowColor() const { return mShadowColor; }
        void setShadowFilter(ShadowFilter filter);
        ShadowFilter shadowFilter() const { return mShadowFilter; }
        void setCullMask(unsigned int mask);
        unsigned int cullMask() const { return mCullMask; }
        // 0..1 mix factor between in-plane and straight-down, for
        // normal-mapped sprites only. See DirectionalLight::height (CanvasTypes.h).
        void setHeight(float height) { mHeight = height; }
        float height() const { return mHeight; }

    protected:
        void onRender(RenderQueue &queue) override;

    private:
        glm::vec4 mColor;
        float mEnergy;
        bool mCastShadow;
        glm::vec4 mShadowColor;
        ShadowFilter mShadowFilter;
        unsigned int mCullMask;
        float mHeight;
    };

}
