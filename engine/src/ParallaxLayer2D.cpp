#include "k2d/ParallaxLayer2D.h"

#include "k2d/RenderQueue.h"
#include "k2d/Texture.h"

#include <cmath>

namespace k2d
{
    ParallaxLayer2D::ParallaxLayer2D()
        : mTexture(nullptr), mMotionScale(1.0f), mTileSize(0.0f),
          mColor(0xFFFFFFFFu), mZIndex(-100)
    {
    }

    void ParallaxLayer2D::setTexture(Texture *texture)
    {
        mTexture = texture;
        if (mTexture && (mTileSize.x <= 0.0f || mTileSize.y <= 0.0f))
            mTileSize = Math::Vec2((float)mTexture->Width(), (float)mTexture->Height());
    }

    void ParallaxLayer2D::setMotionScale(const Math::Vec2 &scale)
    {
        mMotionScale = scale;
    }

    void ParallaxLayer2D::setTileSize(const Math::Vec2 &size)
    {
        mTileSize = size;
    }

    void ParallaxLayer2D::setColor(const Color &color)
    {
        mColor = color;
    }

    void ParallaxLayer2D::setZIndex(int zIndex)
    {
        mZIndex = zIndex;
    }

    void ParallaxLayer2D::submit(RenderQueue &queue, const Camera2D &camera,
                                 float viewportWidth, float viewportHeight) const
    {
        if (!mTexture || mTileSize.x <= 0.0f || mTileSize.y <= 0.0f)
            return;

        float minX = 0.0f;
        float minY = 0.0f;
        float maxX = 0.0f;
        float maxY = 0.0f;
        camera.VisibleRect(minX, minY, maxX, maxY, viewportWidth, viewportHeight);

        Math::Vec2 layerCamera = camera.position * (Math::Vec2(1.0f) - mMotionScale);
        float startX = std::floor((minX - layerCamera.x) / mTileSize.x) * mTileSize.x + layerCamera.x;
        float startY = std::floor((minY - layerCamera.y) / mTileSize.y) * mTileSize.y + layerCamera.y;

        RenderItem &item = queue.AddItem(mZIndex);
        item.blendMode = BLEND_MIX;
        for (float y = startY; y <= maxY; y += mTileSize.y)
        {
            for (float x = startX; x <= maxX; x += mTileSize.x)
            {
                RenderCommand rect = RenderCommand::MakeRect(mTexture->Id(),
                                                               x, y, mTileSize.x, mTileSize.y);
                rect.texWidth = mTexture->Width();
                rect.texHeight = mTexture->Height();
                rect.pivotX = 0.0f;
                rect.pivotY = 0.0f;
                rect.color = mColor;
                item.commands.push_back(rect);
            }
        }
    }

    ParallaxBackground::ParallaxBackground() : mLayers()
    {
    }

    ParallaxBackground::~ParallaxBackground()
    {
        clear();
    }

    ParallaxLayer2D *ParallaxBackground::createLayer(Texture *texture,
                                                     const Math::Vec2 &motionScale,
                                                     int zIndex)
    {
        ParallaxLayer2D *layer = new ParallaxLayer2D();
        layer->setTexture(texture);
        layer->setMotionScale(motionScale);
        layer->setZIndex(zIndex);
        mLayers.push_back(layer);
        return layer;
    }

    void ParallaxBackground::clear()
    {
        for (size_t i = 0; i < mLayers.size(); ++i)
            delete mLayers[i];
        mLayers.clear();
    }

    void ParallaxBackground::submit(RenderQueue &queue, const Camera2D &camera,
                                    float viewportWidth, float viewportHeight) const
    {
        for (size_t i = 0; i < mLayers.size(); ++i)
            mLayers[i]->submit(queue, camera, viewportWidth, viewportHeight);
    }
}