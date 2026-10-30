#pragma once

#include "k2d/Component.h"
#include "k2d/ParallaxLayer2D.h"

namespace k2d
{

    class Texture;

    class ParallaxLayerComponent : public Component
    {
    public:
        static const ComponentType Type = ComponentType::ParallaxLayer;

        ParallaxLayerComponent();

        void setTexture(Texture *texture) { mLayer.setTexture(texture); }
        Texture *texture() const { return mLayer.texture(); }
        void setMotionScale(const Math::Vec2 &scale) { mLayer.setMotionScale(scale); }
        const Math::Vec2 &motionScale() const { return mLayer.motionScale(); }
        void setTileSize(const Math::Vec2 &size) { mLayer.setTileSize(size); }
        const Math::Vec2 &tileSize() const { return mLayer.tileSize(); }
        void setColor(const Color &color) { mLayer.setColor(color); }
        const Color &color() const { return mLayer.color(); }
        void setZIndex(int zIndex) { mLayer.setZIndex(zIndex); }
        int zIndex() const { return mLayer.zIndex(); }

    protected:
        void onRender(RenderQueue &queue) override;

    private:
        ParallaxLayer2D mLayer;
    };

}
