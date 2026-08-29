#pragma once

#include "core/EditorPanel.h"

#include <k2d/CanvasRenderer.h>

namespace k2d
{
class GameObject;
}

namespace k2d::editor
{

class GamePanel final : public EditorPanel
{
public:
    explicit GamePanel(EditorApplication &application);
    ~GamePanel() override;

private:
    void drawAgentPaths(const ImVec2 &position, float width, float height);
    void drawAgentPathsIn(GameObject &object, const Matrix2D &toScreen, const ImVec2 &position,
                          ImDrawList &drawList);
    void drawContents() override;
    void ensureFramebuffer(int width, int height);
    void destroyFramebuffer();
    void renderScene(int width, int height);

    CanvasRenderer mCanvas;
    bool mCanvasInitialized = false;
    bool mCanvasReady = false;
    unsigned int mFramebuffer = 0;
    unsigned int mColorTexture = 0;
    int mFramebufferWidth = 0;
    int mFramebufferHeight = 0;
};

}
