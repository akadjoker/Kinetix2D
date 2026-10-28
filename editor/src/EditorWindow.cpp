#include "EditorWindow.h"

#include "Icons.h"

#include <k2d/ClassDB.h>
#include <k2d/FileSystem.h>
#include <k2d/GameObject.h>
#include <k2d/OpenGL.h>
#include <k2d/Physics2DSerializer.h>
#include <k2d/SceneManager.h>
#include <k2d/Serializer.h>
#include <k2d/ZenScriptComponent.h>

#include <platform.h>

#include <cstdio>

namespace rt = ig::retained;

namespace k2d::editor
{

namespace
{
const char* const kHierarchyPanel = "Hierarchy";
const char* const kScenePanel = "Scene";
const char* const kInspectorPanel = "Inspector";
const char* const kConsolePanel = "Console";

ct::String directoryOf(const char* path)
{
    char buffer[4096];
    path_directory(path, buffer, sizeof buffer);
    return ct::String(buffer);
}

ct::String joinPath(const ct::String& base, const char* relative)
{
    char buffer[4096];
    return path_join(buffer, sizeof buffer, base.c_str(), relative) ? ct::String(buffer) : ct::String(relative);
}
} // namespace

void EditorWindow::build(rt::WidgetApp& app, PlatformWindow* window)
{
    mWindow = window;
    rt::Widget* root = app.addStage("main");
    rt::BoxLayout* outer = root->createChild<rt::BoxLayout>(rt::LayoutDir::Vertical);
    outer->setStretch(1.0f);
    outer->setPadding(0.0f);
    outer->setSpacing(0.0f);

    rt::MenuBar* menuBar = outer->createChild<rt::MenuBar>();
    rt::Menu* file = menuBar->addMenu("File");
    rt::MenuAction* quit = file->addAction("Quit");
    quit->setShortcut("Ctrl+Q");
    quit->triggered.connect([this]() { window_set_should_close(mWindow, true); });
    rt::Menu* view = menuBar->addMenu("View");
    rt::MenuAction* resetLayout = view->addAction("Reset Layout");
    resetLayout->triggered.connect([this]() { this->resetLayout(); });

    // The tools on the left and the run bar centred between two fillers.
    rt::BoxLayout* toolRow = outer->createChild<rt::BoxLayout>(rt::LayoutDir::Horizontal);
    toolRow->setPadding(0.0f);
    toolRow->setSpacing(0.0f);
    mTools = toolRow->createChild<rt::ToolBar>();
    mTools->setDraggable(false);
    mTools->addButton(Icons::get("New"), "New scene");
    mTools->addButton(Icons::get("Open"), "Open scene");
    mTools->addButton(Icons::get("Save"), "Save scene");
    mTools->addSeparator();
    mTools->addButton(Icons::get("Undo"), "Undo");
    mTools->addButton(Icons::get("Redo"), "Redo");
    mTools->addSeparator();
    mTools->addToggle(Icons::get("Select"), "Select", true);
    mTools->addToggle(Icons::get("Move"), "Move");
    mTools->addToggle(Icons::get("Rotate"), "Rotate");
    mTools->addToggle(Icons::get("Scale"), "Scale");
    mTools->addSeparator();
    mTools->addToggle(Icons::get("Grid"), "Grid", true);
    mTools->addToggle(Icons::get("Snap"), "Snap");
    toolRow->createChild<rt::Panel>()->setStretch(1.0f);
    mRunBar = toolRow->createChild<rt::ToolBar>();
    mRunBar->setDraggable(false);
    mPlayItem = mRunBar->addToggle(Icons::get("Play"), "Play");
    mPauseItem = mRunBar->addToggle(Icons::get("Pause"), "Pause");
    mStopItem = mRunBar->addButton(Icons::get("Stop"), "Stop");
    mRunBar->toggleChanged.connect([this](int item, bool on)
    {
        if (item == mPlayItem)
            on ? startPlay() : stopPlay();
        else if (item == mPauseItem)
            setPaused(on);
    });
    mRunBar->buttonClicked.connect([this](int item)
    {
        if (item == mStopItem)
            stopPlay();
    });
    toolRow->createChild<rt::Panel>()->setStretch(1.0f);
    updateRunBar();

    mDock = outer->createChild<rt::DockPanel>();
    mDock->setStretch(1.0f);
    mHierarchy = mDock->addPanel<HierarchyPanel>(kHierarchyPanel);
    mHierarchy->selectionChanged.connect([this](GameObject* object) { showSelection(object); });
    mViewport = mDock->addPanel<ViewportPanel>(kScenePanel);
    mInspector = mDock->addPanel<InspectorPanel>(kInspectorPanel);
    mInspector->show(nullptr);
    mDock->addPanel<rt::Label>(kConsolePanel, "");
    mDock->splitOff(kHierarchyPanel, rt::DockSide::Left, 0.2f);
    mDock->splitOff(kInspectorPanel, rt::DockSide::Right, 0.25f);
    mDock->splitOff(kConsolePanel, rt::DockSide::Bottom, 0.25f);
    mDefaultLayout = mDock->saveLayout();

    mViewport->panned.connect([this](float dx, float dy)
    {
        mCamera.position.x -= dx / mCamera.zoom.x;
        mCamera.position.y -= dy / mCamera.zoom.y;
    });
    mViewport->zoomed.connect([this](float steps, float, float)
    {
        const float factor = steps > 0.0f ? 1.1f : 1.0f / 1.1f;
        mCamera.zoom.x *= factor;
        mCamera.zoom.y *= factor;
    });

    mStatus = outer->createChild<rt::StatusBar>();
    mStatus->setSpacing(16.0f);
    mSceneLabel = mStatus->createChild<rt::Label>("No scene");
    mFpsLabel = mStatus->createChild<rt::Label>("0 fps");
    app.setStage("main");
}

void EditorWindow::resetLayout()
{
    if (mDock)
        mDock->restoreLayout(mDefaultLayout);
}

bool EditorWindow::openScene(const char* path)
{
    static bool registered = false;
    if (!registered)
    {
        RegisterZenScriptSerializer();
        RegisterPhysics2DSerializers();
        RegisterCoreClasses();
        registered = true;
    }
    const ct::String sceneDirectory = directoryOf(path);
    const ct::String projectRoot = directoryOf(sceneDirectory.c_str());
    FileSystem& files = FileSystem::Instance();
    files.AddSearchPath(projectRoot.c_str());
    files.AddSearchPath(joinPath(projectRoot, "assets").c_str());
    files.AddSearchPath(sceneDirectory.c_str());

    stopPlay();
    mScene.clear();
    mScene.setSimulationEnabled(false);
    if (!GetSceneManager().Load(mScene, mAssets, path))
        return false;
    mTopology = mScene.topologyVersion();
    mSceneLabel->setText(path_filename(path));
    mHierarchy->rebuild(mScene);
    showSelection(nullptr);
    return true;
}

void EditorWindow::frame(float deltaTime, float pixelScale)
{
    mFpsTime += deltaTime;
    ++mFpsFrames;
    if (mFpsTime >= 1.0f)
    {
        char text[32];
        std::snprintf(text, sizeof text, "%.0f fps", mFpsFrames / mFpsTime);
        mFpsLabel->setText(text);
        mFpsTime = 0.0f;
        mFpsFrames = 0;
    }
    mStatus->layout();

    if (mPlaying && !mPaused)
    {
        SetZenScriptFrameStats(deltaTime, mFpsTime > 0.0f ? mFpsFrames / mFpsTime : 0.0f);
        mScene.update(deltaTime);
        DispatchZenScriptEvents(mScene.root());
        if (GetSceneManager().HasRequest() && GetSceneManager().ApplyRequest(mScene, mAssets))
            mScene.setSimulationEnabled(true);
    }
    // Scripts spawn and destroy objects while playing: the tree follows, and the
    // inspector lets go of an object that may be gone.
    if (mScene.topologyVersion() != mTopology)
    {
        mTopology = mScene.topologyVersion();
        mHierarchy->rebuild(mScene);
        showSelection(nullptr);
    }

    if (!mViewport || !mViewport->isVisible())
        return;
    const rt::Rect area = mViewport->absoluteRect();
    renderScene(area.w, area.h, pixelScale);
}

void EditorWindow::renderScene(float width, float height, float pixelScale)
{
    if (!mCanvasReady && !mCanvasFailed)
    {
        mCanvasReady = mCanvas.Init();
        mCanvasFailed = !mCanvasReady;
    }
    const int pixelWidth = static_cast<int>(width * pixelScale + 0.5f);
    const int pixelHeight = static_cast<int>(height * pixelScale + 0.5f);
    if (!mCanvasReady || !mTarget.resize(pixelWidth, pixelHeight))
        return;

    mTarget.bind();
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.12f, 0.12f, 0.13f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    mCanvas.SetProjection(mCamera.Projection(width, height));
    mScene.setRenderCamera(&mCamera, width, height);
    mScene.render(mCanvas);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    rt::TextureHandle texture;
    texture.value = mTarget.texture();
    mViewport->setTexture(texture);
}

void EditorWindow::showSelection(GameObject* object)
{
    mInspector->show(object);
}

void EditorWindow::startPlay()
{
    if (mPlaying)
        return;
    // Play simulates the scene in place, as the legacy editor did; Stop throws
    // away what physics and scripts changed by reading the snapshot back.
    mPlaySnapshot = Serializer::WriteObject(mScene.root(), &mAssets);
    mEditGravity = mScene.gravity();
    GetSceneManager().ClearRequest();
    SetZenScriptAssets(&mAssets);
    RouteZenScriptCollisions(mScene);
    RouteZenScriptAnimationEvents(mScene);
    RouteZenScriptActionEvents(mScene);
    RouteZenScriptParticleHits(mScene);
    SetZenScriptsEnabled(true);
    mScene.setSimulationEnabled(true);
    mPlaying = true;
    mPaused = false;
    updateRunBar();
}

void EditorWindow::stopPlay()
{
    if (!mPlaying)
        return;
    SetZenScriptsEnabled(false);
    GetSceneManager().ClearRequest();
    showSelection(nullptr);
    mScene.clear();
    mScene.setSimulationEnabled(false);
    mScene.setGravity(mEditGravity);
    Serializer::ReadInto(mScene, mScene.root(), mPlaySnapshot, &mAssets);
    mPlaySnapshot = ct::Json();
    mPlaying = false;
    mPaused = false;
    mTopology = mScene.topologyVersion();
    mHierarchy->rebuild(mScene);
    updateRunBar();
}

void EditorWindow::setPaused(bool paused)
{
    mPaused = mPlaying && paused;
    updateRunBar();
}

void EditorWindow::updateRunBar()
{
    mRunBar->setItemChecked(mPlayItem, mPlaying);
    mRunBar->setItemChecked(mPauseItem, mPaused);
    mRunBar->setItemEnabled(mPauseItem, mPlaying);
    mRunBar->setItemEnabled(mStopItem, mPlaying);
}

void EditorWindow::shutdown()
{
    stopPlay();
    mTarget.destroy();
    if (mCanvasReady)
        mCanvas.Shutdown();
    mCanvasReady = false;
}

} // namespace k2d::editor
