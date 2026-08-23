#pragma once

#include "k2d/Scene.h"

#include <glm/glm.hpp>

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

        // Sets a screen-space ortho projection (0,0)-(screenWidth,screenHeight)
        // on `canvas` and draws this layer's scene. Leaves that projection set
        // on `canvas` afterward -- set canvas.SetProjection/SetOrtho again
        // before drawing more world-space content.
        void render(CanvasRenderer &canvas, float screenWidth, float screenHeight);

    private:
        Scene mScene;
    };

}
