#pragma once

#include "k2d/Camera2D.h"
#include "k2d/CanvasTypes.h"
#include "k2d/Color.h"

#include <mathc.h>
#include <ct/vector.hpp>

namespace k2d
{
    class RenderQueue;
    class Texture;

    class ParallaxLayer2D
    {
    public:
        ParallaxLayer2D();

        void setTexture(Texture *texture);
        void setMotionScale(const Math::Vec2 &scale);
        void setTileSize(const Math::Vec2 &size);
        void setColor(const Color &color);
        void setZIndex(int zIndex);

        Texture *texture() const { return mTexture; }
        const Math::Vec2 &motionScale() const { return mMotionScale; }
        const Math::Vec2 &tileSize() const { return mTileSize; }
        const Color &color() const { return mColor; }
        int zIndex() const { return mZIndex; }

        void submit(RenderQueue &queue, const Camera2D &camera,
                    float viewportWidth, float viewportHeight) const;

    private:
        Texture *mTexture;
        Math::Vec2 mMotionScale;
        Math::Vec2 mTileSize;
        Color mColor;
        int mZIndex;
    };

    class ParallaxBackground
    {
    public:
        ParallaxBackground();
        ~ParallaxBackground();

        ParallaxBackground(const ParallaxBackground &) = delete;
        ParallaxBackground &operator=(const ParallaxBackground &) = delete;

        ParallaxLayer2D *createLayer(Texture *texture, const Math::Vec2 &motionScale,
                                     int zIndex = -100);
        void clear();
        std::size_t layerCount() const { return mLayers.size(); }

        void submit(RenderQueue &queue, const Camera2D &camera,
                    float viewportWidth, float viewportHeight) const;

    private:
        ct::Vector<ParallaxLayer2D *> mLayers;
    };
}