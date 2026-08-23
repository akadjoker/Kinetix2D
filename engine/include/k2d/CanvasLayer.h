#pragma once

#include "k2d/Scene.h"

#include <mathc.h>

namespace k2d
{

    class CanvasRenderer;

    class CanvasLayer
    {
    public:
        CanvasLayer() = default;

        CanvasLayer(const CanvasLayer &) = delete;
        CanvasLayer &operator=(const CanvasLayer &) = delete;

        Scene &scene() { return mScene; }
        const Scene &scene() const { return mScene; }
        GameObject &root() { return mScene.root(); }

        void update(float deltaTime) { mScene.update(deltaTime); }

        void render(CanvasRenderer &canvas, float screenWidth, float screenHeight);

    private:
        Scene mScene;
    };

}