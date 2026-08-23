#pragma once

#include "k2d/Camera2D.h"
#include "k2d/CanvasTypes.h"

#include <glm/glm.hpp>
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
        void setMotionScale(const glm::vec2 &scale);
        void setTileSize(const glm::vec2 &size);
        void setColor(unsigned int color);
        void setZIndex(int zIndex);

        Texture *texture() const { return mTexture; }
        const glm::vec2 &motionScale() const { return mMotionScale; }
        const glm::vec2 &tileSize() const { return mTileSize; }
        int zIndex() const { return mZIndex; }

        void submit(RenderQueue &queue, const Camera2D &camera,
                    float viewportWidth, float viewportHeight) const;

    private:
        Texture *mTexture;
        glm::vec2 mMotionScale;
        glm::vec2 mTileSize;
        unsigned int mColor;
        int mZIndex;
    };

    class ParallaxBackground
    {
    public:
        ParallaxBackground();
        ~ParallaxBackground();

        ParallaxBackground(const ParallaxBackground &) = delete;
        ParallaxBackground &operator=(const ParallaxBackground &) = delete;

        ParallaxLayer2D *createLayer(Texture *texture, const glm::vec2 &motionScale,
                                     int zIndex = -100);
        void clear();
        std::size_t layerCount() const { return mLayers.size(); }

        void submit(RenderQueue &queue, const Camera2D &camera,
                    float viewportWidth, float viewportHeight) const;

    private:
        ct::Vector<ParallaxLayer2D *> mLayers;
    };
}