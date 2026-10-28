#pragma once

#include "AnimatorPanel.h"
#include "AssetsPanel.h"
#include "HierarchyPanel.h"
#include "InspectorPanel.h"
#include "SceneGizmo.h"
#include "SpriteEditorPanel.h"
#include "TilePainterPanel.h"
#include "ViewportPanel.h"
#include "core/CommandStack.h"

#include "BasicWidgets.hpp"
#include "ConsoleWidget.hpp"
#include "DockPanel.hpp"
#include "FileDialog.hpp"
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
    void newScene();
    bool saveScene(const char* path);
    void frame(float deltaTime, float pixelScale);
    void shutdown();

private:
    void resetLayout();
    void registerTypes();
    void drainLog();
    void onShortcut(ig::retained::KeyEvent& e);
    void setTool(SceneTool tool);
    void showOpenDialog();
    void showSaveDialog();
    void save();
    void markDirty(bool dirty);
    // Scene state that starts again for a scene just opened or made.
    void sceneLoaded();
    bool ensureCanvas();
    void renderScene(float width, float height, float pixelScale);
    void renderGame(const ig::retained::Rect& area, float pixelScale);
    void select(GameObject* object);
    // The panels that show the selected object, shown it again.
    void showObject(GameObject* object);
    void addComponent(const char* type);
    void refreshTree();
    ct::Json snapshot() const;
    void restore(const ct::Json& state);
    void commit(const char* label);
    void undo();
    void redo();
    void updateUndoButtons();
    void runHierarchyAction(HierarchyAction action);
    void dropAsset(const ct::String& path, float x, float y);
    // A sprite object showing the texture, or the region of it when rect has a
    // size, centred on (x, y) in the scene view.
    void addSprite(const ct::String& texture, const ct::String& name, const Math::Vec4& rect, float x, float y);
    void dropObject(GameObject* object, GameObject* target, ig::retained::TreeView::DropZone zone);
    void startPlay();
    void stopPlay();
    void setPaused(bool paused);
    void updateRunBar();

    PlatformWindow* mWindow = nullptr;
    ig::retained::DockPanel* mDock = nullptr;
    ig::retained::ToolBar* mTools = nullptr;
    ig::retained::ToolBar* mRunBar = nullptr;
    int mToolItems[4] = {-1, -1, -1, -1};
    int mGridItem = -1;
    int mSnapItem = -1;
    SceneGizmo mGizmo;
    int mUndoItem = -1;
    int mRedoItem = -1;
    int mPlayItem = -1;
    int mPauseItem = -1;
    int mStopItem = -1;
    ig::retained::StatusBar* mStatus = nullptr;
    ig::retained::Label* mSceneLabel = nullptr;
    ig::retained::Label* mFpsLabel = nullptr;
    ViewportPanel* mViewport = nullptr;
    ViewportPanel* mGame = nullptr;
    HierarchyPanel* mHierarchy = nullptr;
    InspectorPanel* mInspector = nullptr;
    AssetsPanel* mAssetsPanel = nullptr;
    SpriteEditorPanel* mSpriteEditor = nullptr;
    AnimatorPanel* mAnimator = nullptr;
    TilePainterPanel* mTilePainter = nullptr;
    ig::retained::ConsoleWidget* mConsole = nullptr;
    ig::retained::String mDefaultLayout;

    Assets mAssets;
    ct::String mProjectRoot;
    ig::retained::WidgetApp* mApp = nullptr;
    // The scene file as read: saving writes it back with only "root" replaced,
    // so what the editor does not edit (physics, cursor, ...) is kept.
    ct::String mScenePath;
    ct::Json mSceneDocument;
    bool mDirty = false;
    Scene mScene;
    CanvasRenderer mCanvas;
    RenderTarget mTarget;
    RenderTarget mGameTarget;
    // The camera the Game view falls back to when the scene has none active.
    Camera2D mGameCamera;
    Camera2D mCamera;
    bool mCanvasReady = false;
    bool mCanvasFailed = false;
    bool mPlaying = false;
    bool mPaused = false;
    ct::Json mPlaySnapshot;
    Math::Vec2 mEditGravity;
    uint32_t mTopology = 0;

    // The selection survives undo by its path of child indices: a restored scene
    // gets new objects, and new ids.
    GameObject* mSelected = nullptr;
    bool mInspectorStale = false;
    CommandStack mCommands;
    ct::Json mCommitted;
    ct::String mPendingEdit;
    float mFpsTime = 0.0f;
    int mFpsFrames = 0;
};

} // namespace k2d::editor
