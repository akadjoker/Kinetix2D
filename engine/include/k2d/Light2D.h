#pragma once

#include "k2d/Component.h"
#include "k2d/CanvasTypes.h"

#include <glm/glm.hpp>

namespace k2d
{

    // Godot's Light2D (point light). Registers a PointLight into the router each
    // frame; the canvas renderer accumulates it additively in the canvas shader.
    class Light2D : public Component
    {
    public:
        static const ComponentType Type = ComponentType::Light;

        Light2D();

        void setColor(float r, float g, float b, float a = 1.0f);
        void setEnergy(float energy);
        void setRadius(float radius);
        void setCastShadow(bool castShadow);
        void setShadowColor(float r, float g, float b, float a = 1.0f);
        void setShadowFilter(ShadowFilter filter);

    protected:
        void onRender(RenderQueue &queue) override;

    private:
        glm::vec4 mColor;
        float mEnergy;
        float mRadius;
        bool mCastShadow;
        glm::vec4 mShadowColor;
        ShadowFilter mShadowFilter;
    };

}
