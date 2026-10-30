#pragma once

#include "k2d/Component.h"
#include "k2d/CanvasTypes.h"
#include "k2d/Color.h"

#include <mathc.h>

namespace k2d
{

    class Light2D : public Component
    {
    public:
        static const ComponentType Type = ComponentType::Light;

        Light2D();

        void setColor(float r, float g, float b, float a = 1.0f);
        const Color &color() const { return mColor; }
        void setEnergy(float energy);
        float energy() const { return mEnergy; }
        void setRadius(float radius);
        float radius() const { return mRadius; }
        void setCastShadow(bool castShadow);
        bool castShadow() const { return mCastShadow; }
        void setShadowColor(float r, float g, float b, float a = 1.0f);
        const Color &shadowColor() const { return mShadowColor; }
        void setShadowFilter(ShadowFilter filter);
        ShadowFilter shadowFilter() const { return mShadowFilter; }
        void setFalloff(float falloff);
        float falloff() const { return mFalloff; }
        void setShadowBias(float bias);
        float shadowBias() const { return mShadowBias; }
        void setShadowSmooth(float smooth);
        float shadowSmooth() const { return mShadowSmooth; }

        void setCullMask(unsigned int mask);
        unsigned int cullMask() const { return mCullMask; }

        void setHeight(float height) { mHeight = height; }
        float height() const { return mHeight; }

    protected:
        void onRender(RenderQueue &queue) override;

    private:
        Color mColor;
        float mEnergy;
        float mRadius;
        bool mCastShadow;
        Color mShadowColor;
        ShadowFilter mShadowFilter;
        unsigned int mCullMask;
        float mHeight;
        float mFalloff;
        float mShadowBias;
        float mShadowSmooth;
    };

    template <> struct ComponentMatch<Light2D>
    {
        static bool test(const Component *component)
        {
            return component->componentClass() == ComponentClass::Light2D;
        }
    };

}