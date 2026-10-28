#pragma once

#include "AnimatorPanel.h"
#include "AssetsPanel.h"
#include "HierarchyPanel.h"
#include "InspectorPanel.h"
#include "McpServer.h"
#include "ParticlesPanel.h"
#include "ProfilerPanel.h"
#include "SceneGizmo.h"
#include "SettingsPanel.h"
#include "ScriptEditorPanel.h"
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
#include <k2d/Input.h>
#include <k2d/CanvasRenderer.h>
#include <k2d/RenderTarget.h>
#include <k2d/Scene.h>

#include <ct/json.hpp>
#include <ct/string.hpp>

#include <platform.h>

namespace k2d::editor
{

class EditorWindow
{
    friend class McpServer;

public:
    void build(ig::retained::WidgetApp& app, PlatformWindow* window);
    bool openScene(const char* path);
    void newScene();
    bool saveScene(const char* path);
    void frame(float deltaTime, float pixelScale);
    void shutdown();
    // The MCP server on 127.0.0.1:port; off when port is 0.
    bool setMcp(std::uint16_t port);
    // A platform event, as the window reads it: during Play it reaches the game.
    void onPlatformEvent(const Event& event);

private:
    void resetLayout();
    void registerTypes();
    void drainLog();
    void loadPhysics();
    void applyPhysics();
    void physicsChanged(const PhysicsSettings& settings, bool project);
    // Particles, animations and action sequences moving in edit mode.
    void tickEditPreview(GameObject& object, float deltaTime);
    void restartEditPreview(GameObject& object);
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
    // Adds the script to the object, or to a new object at (x, y) in the scene
    // view when there is none.
    void attachScript(const ct::String& path, GameObject* object, float x, float y);
    // An instance of the prefab under parent (the top when nullptr); at (x, y)
    // in the scene view when placed, where the prefab had it otherwise.
    GameObject* instantiatePrefab(const ct::String& path, GameObject* parent, bool placed, float x, float y);
    void showSavePrefabDialog(GameObject* object);
    bool savePrefab(GameObject* object, const ct::String& path);
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
    ScriptEditorPanel* mScriptEditor = nullptr;
    ParticlesPanel* mParticles = nullptr;
    SettingsPanel* mSettings = nullptr;
    ProfilerPanel* mProfiler = nullptr;
    PhysicsSettings mPhysics;
    ct::Json mProjectDocument;   // project.k2dproj as read, written back with only physics changed
    int mPreviewItem = -1;
    bool mLivePreview = true;
    McpServer mMcp{*this};
    ig::retained::Label* mMcpLabel = nullptr;
    ig::retained::MenuAction* mMcpAction = nullptr;
    // What the game's scripts and UI read while it plays in the Game view.
    Input mGameInput;
    // Keys tapped from outside (MCP), let go after one game update.
    ct::Vector<int> mPendingReleases;
    // Where the game view showed the game last frame: window points and the
    // game's own size, to turn game coordinates into the window's.
    float mGameViewX = 0.0f, mGameViewY = 0.0f, mGameViewW = 0.0f, mGameViewH = 0.0f;
    float mGameVirtualW = 0.0f, mGameVirtualH = 0.0f;
    // The console's lines, numbered, for tools that read them back (MCP).
    ct::Vector<ct::String> mLogHistory;
    std::size_t mLogFirst = 0;   // the number of the oldest line kept
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
