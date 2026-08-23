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
        const glm::vec4 &color() const { return mColor; }
        void setEnergy(float energy);
        float energy() const { return mEnergy; }
        void setRadius(float radius);
        float radius() const { return mRadius; }
        void setCastShadow(bool castShadow);
        bool castShadow() const { return mCastShadow; }
        void setShadowColor(float r, float g, float b, float a = 1.0f);
        const glm::vec4 &shadowColor() const { return mShadowColor; }
        void setShadowFilter(ShadowFilter filter);
        ShadowFilter shadowFilter() const { return mShadowFilter; }
        // Godot's range_item_cull_mask: only illuminates items whose light
        // mask shares a bit with this. Default 1 (bit 0).
        void setCullMask(unsigned int mask);
        unsigned int cullMask() const { return mCullMask; }
        // Z offset above the 2D plane, for normal-mapped sprites only. See
        // PointLight::height (CanvasTypes.h).
        void setHeight(float height) { mHeight = height; }
        float height() const { return mHeight; }

    protected:
        void onRender(RenderQueue &queue) override;

    private:
        glm::vec4 mColor;
        float mEnergy;
        float mRadius;
        bool mCastShadow;
        glm::vec4 mShadowColor;
        ShadowFilter mShadowFilter;
        unsigned int mCullMask;
        float mHeight;
    };

}
