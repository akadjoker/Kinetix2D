#pragma once

#include "core/EditorPanel.h"

#include <k2d/CanvasRenderer.h>
#include <k2d/RenderQueue.h>

namespace k2d
{
class ParticleComponent;
}

namespace k2d::editor
{

class ParticlePanel final : public EditorPanel
{
public:
    explicit ParticlePanel(EditorApplication &application);
    ~ParticlePanel() override;

private:
    void drawContents() override;
    void ensureFramebuffer(int width, int height);
    void destroyFramebuffer();
    void renderPreview(ParticleComponent &particle, int width, int height);

    float mZoom = 1.0f;
    bool mShowEmitter = true;

    CanvasRenderer mCanvas;
    RenderQueue mQueue;
    bool mCanvasInitialized = false;
    bool mCanvasReady = false;
    unsigned int mFramebuffer = 0;
    unsigned int mColorTexture = 0;
    int mFramebufferWidth = 0;
    int mFramebufferHeight = 0;
};

}
