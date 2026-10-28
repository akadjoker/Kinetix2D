#pragma once

#include "HierarchyPanel.h"
#include "InspectorPanel.h"
#include "ViewportPanel.h"

#include "BasicWidgets.hpp"
#include "DockPanel.hpp"
#include "LayoutWidgets.hpp"
#include "MenuWidgets.hpp"
#include "ToolBar.hpp"
#include "WidgetApp.hpp"

#include <k2d/Assets.h>
#include <k2d/Camera2D.h>
#include <k2d/CanvasRenderer.h>
#include <k2d/RenderTarget.h>
#include <k2d/Scene.h>

#include <ct/json.hpp>
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
    void startPlay();
    void stopPlay();
    void setPaused(bool paused);
    void updateRunBar();

    PlatformWindow* mWindow = nullptr;
    ig::retained::DockPanel* mDock = nullptr;
    ig::retained::ToolBar* mTools = nullptr;
    ig::retained::ToolBar* mRunBar = nullptr;
    int mPlayItem = -1;
    int mPauseItem = -1;
    int mStopItem = -1;
    ig::retained::StatusBar* mStatus = nullptr;
    ig::retained::Label* mSceneLabel = nullptr;
    ig::retained::Label* mFpsLabel = nullptr;
    ViewportPanel* mViewport = nullptr;
    HierarchyPanel* mHierarchy = nullptr;
    InspectorPanel* mInspector = nullptr;
    ig::retained::String mDefaultLayout;

    Assets mAssets;
    Scene mScene;
    CanvasRenderer mCanvas;
    RenderTarget mTarget;
    Camera2D mCamera;
    bool mCanvasReady = false;
    bool mCanvasFailed = false;
    bool mPlaying = false;
    bool mPaused = false;
    ct::Json mPlaySnapshot;
    Math::Vec2 mEditGravity;
    uint32_t mTopology = 0;
    float mFpsTime = 0.0f;
    int mFpsFrames = 0;
};

} // namespace k2d::editor
