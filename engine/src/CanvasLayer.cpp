#include "k2d/CanvasLayer.h"

#include "k2d/CanvasRenderer.h"

namespace k2d
{

    void CanvasLayer::render(CanvasRenderer &canvas, float screenWidth, float screenHeight)
    {
        canvas.SetOrtho(screenWidth, screenHeight);
        mScene.render(canvas);
    }

}