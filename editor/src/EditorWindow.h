#pragma once

#include "HierarchyPanel.h"
#include "ViewportPanel.h"

#include "BasicWidgets.hpp"
#include "DockPanel.hpp"
#include "LayoutWidgets.hpp"
#include "MenuWidgets.hpp"
#include "WidgetApp.hpp"

#include <k2d/Assets.h>
#include <k2d/Camera2D.h>
#include <k2d/CanvasRenderer.h>
#include <k2d/RenderTarget.h>
#include <k2d/Scene.h>

#include <ct/string.hpp>

struct PlatformWindow;

namespace k2d::editor
{

class EditorWindow
{
public:
    void build(ig::retained::WidgetApp& app, PlatformWindow* window);
    bool openScene(const char* path);
    void frame(float deltaTime, float pixelScale);
    void shutdown();

private:
    void resetLayout();
    void renderScene(float width, float height, float pixelScale);
    void showSelection(GameObject* object);

    PlatformWindow* mWindow = nullptr;
    ig::retained::DockPanel* mDock = nullptr;
    ig::retained::StatusBar* mStatus = nullptr;
    ig::retained::Label* mSceneLabel = nullptr;
    ig::retained::Label* mFpsLabel = nullptr;
    ViewportPanel* mViewport = nullptr;
    HierarchyPanel* mHierarchy = nullptr;
    ig::retained::Label* mInspector = nullptr;
    ig::retained::String mDefaultLayout;

    Assets mAssets;
    Scene mScene;
    CanvasRenderer mCanvas;
    RenderTarget mTarget;
    Camera2D mCamera;
    bool mCanvasReady = false;
    bool mCanvasFailed = false;
    float mFpsTime = 0.0f;
    int mFpsFrames = 0;
};

} // namespace k2d::editor
