#pragma once

#include "k2d/Scene.h"

#include <glm/glm.hpp>

namespace k2d
{

    class CanvasRenderer;

    // Godot's CanvasLayer: a group of canvas items drawn in a fixed
    // screen-space transform, independent of whatever Camera2D projection the
    // main scene uses -- a HUD, a menu, anything that shouldn't pan/zoom with
    // the game world. Implemented as its own Scene (own GameObject tree, own
    // RenderQueue) rendered through a second CanvasRenderer::DrawItems pass
    // with a plain screen ortho projection; verified (canvaslayer_diag.cpp)
    // that two DrawItems passes with different projections compose cleanly
    // in one frame with no shared state leaking between them, so this needed
    // no CanvasRenderer/shader changes at all.
    //
    // Usage, in draw order (later draws on top):
    //   worldCanvasLayer... // not this class -- just canvas.SetProjection(camera...); scene.render(canvas);
    //   hud.update(dt);
    //   hud.render(canvas, (float)device.Width(), (float)device.Height());
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
