#include "EditorWindow.h"

#include "Icons.h"

#include "TextInputWidgets.hpp"

#include <k2d/ClassDB.h>
#include <k2d/FileBuffer.h>
#include <k2d/FileSystem.h>
#include <k2d/ActionSequence2D.h>
#include <k2d/Animation2D.h>
#include <k2d/CameraComponent.h>
#include <k2d/Device.h>
#include <k2d/ParticleComponent.h>
#include <k2d/Prefab.h>
#include <k2d/Profiler.h>
#include <k2d/ZenRuntime.h>
#include <k2d/GameObject.h>
#include <k2d/GameViewport.h>
#include <k2d/OpenGL.h>
#include <k2d/Physics2DSerializer.h>
#include <k2d/SceneManager.h>
#include <k2d/SpriteComponent.h>
#include <k2d/Texture.h>
#include "core/EditorFileSystem.h"
#include <k2d/ScreenFade.h>
#include <k2d/UiControls.h>
#include <k2d/Serializer.h>
#include <k2d/ZenScriptComponent.h>

#include <platform.h>

#include <cstdio>
#include <mutex>

namespace rt = ig::retained;

namespace k2d::editor
{

namespace
{
const char* const kHierarchyPanel = "Hierarchy";
const char* const kScenePanel = "Scene";
const char* const kGamePanel = "Game";
const char* const kInspectorPanel = "Inspector";
const char* const kConsolePanel = "Console";
const char* const kAssetsPanel = "Assets";
const char* const kSpritePanel = "Sprite Editor";
const char* const kAnimatorPanel = "Animator";
const char* const kTilePanel = "Tile Painter";
const char* const kScriptPanel = "Script Editor";
const char* const kParticlesPanel = "Particles";
const char* const kSettingsPanel = "Settings";
const char* const kProfilerPanel = "Profiler";

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

// Engine logs and script output, queued from any thread and shown by the
// Console on the main one. They still reach stderr as before.
struct PendingLine
{
    rt::LogLevel level;
    ct::String text;
};
std::mutex gLogMutex;
ct::Vector<PendingLine> gLogLines;

void queueLine(rt::LogLevel level, const char* text)
{
    std::lock_guard<std::mutex> lock(gLogMutex);
    PendingLine line;
    line.level = level;
    line.text = text ? text : "";
    gLogLines.push_back(line);
}

void editorLog(::LogLevel level, const char* message, void*)
{
    static const char* const names[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    const int index = level >= LOGLEVEL_DEBUG && level <= LOGLEVEL_ERROR ? static_cast<int>(level) : 1;
    std::fprintf(stderr, "[%s] %s\n", names[index], message);
    const rt::LogLevel shown = level == LOGLEVEL_ERROR  ? rt::LogLevel::Error
                               : level == LOGLEVEL_WARN ? rt::LogLevel::Warn
                               : level == LOGLEVEL_DEBUG ? rt::LogLevel::Trace
                                                         : rt::LogLevel::Info;
    queueLine(shown, message);
}

void scriptOutput(const char* text, bool isError, void*)
{
    std::fprintf(isError ? stderr : stdout, "%s", text ? text : "");
    // print() hands its newline along; the console keeps one line per call.
    ct::String line = text ? text : "";
    while (!line.empty() && (line[line.size() - 1] == '\n' || line[line.size() - 1] == '\r'))
        line = line.substr(0, line.size() - 1);
    queueLine(isError ? rt::LogLevel::Error : rt::LogLevel::Info, line.c_str());
}

// The child indices from the scene root down to the object.
ct::Vector<std::size_t> pathOf(const GameObject* object)
{
    ct::Vector<std::size_t> path;
    while (object && object->parent())
    {
        path.push_back(object->parent()->childIndex(object));
        object = object->parent();
    }
    ct::Vector<std::size_t> reversed;
    for (std::size_t i = path.size(); i > 0; --i)
        reversed.push_back(path[i - 1]);
    return reversed;
}

GameObject* objectAt(GameObject& root, const ct::Vector<std::size_t>& path)
{
    GameObject* object = &root;
    for (std::size_t i = 0; i < path.size(); ++i)
    {
        if (path[i] >= object->childCount())
            return nullptr;
        object = object->child(path[i]);
    }
    return path.empty() ? nullptr : object;
}
} // namespace

void EditorWindow::build(rt::WidgetApp& app, PlatformWindow* window)
{
    mWindow = window;
    mApp = &app;
    rt::Widget* root = app.addStage("main");
    rt::BoxLayout* outer = root->createChild<rt::BoxLayout>(rt::LayoutDir::Vertical);
    outer->setStretch(1.0f);
    outer->setPadding(0.0f);
    outer->setSpacing(0.0f);

    rt::MenuBar* menuBar = outer->createChild<rt::MenuBar>();
    rt::Menu* file = menuBar->addMenu("File");
    file->addAction("New Scene", [this]() { newScene(); })->setShortcut("Ctrl+N");
    file->addAction("Open Scene...", [this]() { showOpenDialog(); })->setShortcut("Ctrl+O");
    file->addAction("Save", [this]() { save(); })->setShortcut("Ctrl+S");
    file->addAction("Save As...", [this]() { showSaveDialog(); });
    file->addSeparator();
    rt::MenuAction* quit = file->addAction("Quit");
    quit->setShortcut("Ctrl+Q");
    quit->triggered.connect([this]() { window_set_should_close(mWindow, true); });
    rt::Menu* edit = menuBar->addMenu("Edit");
    edit->addAction("Undo", [this]() { undo(); })->setShortcut("Ctrl+Z");
    edit->addAction("Redo", [this]() { redo(); })->setShortcut("Ctrl+Y");
    rt::Menu* tools = menuBar->addMenu("Tools");
    mMcpAction = tools->addCheckable("MCP Server (127.0.0.1:7777)", false);
    mMcpAction->triggered.connect([this]() { setMcp(mMcp.running() ? 0 : 7777); });
    rt::Menu* view = menuBar->addMenu("View");
    rt::MenuAction* resetLayout = view->addAction("Reset Layout");
    resetLayout->triggered.connect([this]() { this->resetLayout(); });

    // The tools on the left and the run bar centred between two fillers.
    rt::BoxLayout* toolRow = outer->createChild<rt::BoxLayout>(rt::LayoutDir::Horizontal);
    toolRow->setPadding(0.0f);
    toolRow->setSpacing(0.0f);
    mTools = toolRow->createChild<rt::ToolBar>();
    mTools->setDraggable(false);
    const int newItem = mTools->addButton(Icons::get("New"), "New scene");
    const int openItem = mTools->addButton(Icons::get("Open"), "Open scene");
    const int saveItem = mTools->addButton(Icons::get("Save"), "Save scene");
    mTools->buttonClicked.connect([this, newItem, openItem, saveItem](int item)
    {
        if (item == newItem)
            newScene();
        else if (item == openItem)
            showOpenDialog();
        else if (item == saveItem)
            save();
    });
    mTools->addSeparator();
    mUndoItem = mTools->addButton(Icons::get("Undo"), "Undo");
    mRedoItem = mTools->addButton(Icons::get("Redo"), "Redo");
    mTools->buttonClicked.connect([this](int item)
    {
        if (item == mUndoItem)
            undo();
        else if (item == mRedoItem)
            redo();
    });
    mTools->addSeparator();
    mToolItems[0] = mTools->addToggle(Icons::get("Select"), "Select", true);
    mToolItems[1] = mTools->addToggle(Icons::get("Move"), "Move");
    mToolItems[2] = mTools->addToggle(Icons::get("Rotate"), "Rotate");
    mToolItems[3] = mTools->addToggle(Icons::get("Scale"), "Scale");
    mTools->addSeparator();
    mPreviewItem = mTools->addToggle(Icons::get("Particle"),
                                     "Live preview: particles and animations move while editing", true);
    mGridItem = mTools->addToggle(Icons::get("Grid"), "Grid", true);
    mSnapItem = mTools->addToggle(Icons::get("Snap"), "Snap");
    mTools->toggleChanged.connect([this](int item, bool on)
    {
        if (item == mPreviewItem)
        {
            mLivePreview = on;
            restartEditPreview(mScene.root());
        }
        else if (item == mGridItem)
            mGizmo.setGrid(on);
        else if (item == mSnapItem)
            mGizmo.setSnap(on);
        // The four tools are one choice: the one pressed stays on.
        for (int i = 0; i < 4; ++i)
            if (item == mToolItems[i])
                mGizmo.setTool(static_cast<SceneTool>(i));
        for (int i = 0; i < 4; ++i)
            mTools->setItemChecked(mToolItems[i], static_cast<int>(mGizmo.tool()) == i);
    });
    toolRow->createChild<rt::Panel>()->setStretch(1.0f);
    mRunBar = toolRow->createChild<rt::ToolBar>();
    mRunBar->setDraggable(false);
    mPlayItem = mRunBar->addToggle(Icons::get("Play"), "Play");
    mPauseItem = mRunBar->addToggle(Icons::get("Pause"), "Pause");
    mStopItem = mRunBar->addButton(Icons::get("Stop"), "Stop");
    mRunBar->addSeparator();
    mWindowItem = mRunBar->addButton(Icons::get("Export"), "Play in a window: the saved scene in the runner (F6)");
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
        else if (item == mWindowItem)
            runInWindow();
    });
    toolRow->createChild<rt::Panel>()->setStretch(1.0f);
    updateRunBar();
    updateUndoButtons();

    mDock = outer->createChild<rt::DockPanel>();
    mDock->setStretch(1.0f);
    mHierarchy = mDock->addPanel<HierarchyPanel>(kHierarchyPanel);
    mHierarchy->selectionChanged.connect([this](GameObject* object)
    {
        mSelected = object;
        showObject(object);
    });
    mHierarchy->actionRequested.connect([this](HierarchyAction action) { runHierarchyAction(action); });
    mHierarchy->edited.connect([this](const char* label) { commit(label); });
    mHierarchy->assetDropped.connect([this](const ct::String& path, GameObject* object)
    {
        const rt::Rect area = mViewport->absoluteRect();
        if (isPrefabPath(path))
            instantiatePrefab(path, object, false, 0.0f, 0.0f);
        else
            attachScript(path, object, area.w * 0.5f, area.h * 0.5f);
    });
    mHierarchy->dropped.connect([this](GameObject* object, GameObject* target, rt::TreeView::DropZone zone)
    {
        dropObject(object, target, zone);
    });
    mViewport = mDock->addPanel<ViewportPanel>(kScenePanel);
    mGame = mDock->addPanel<ViewportPanel>(kGamePanel);
    mSpriteEditor = mDock->addPanel<SpriteEditorPanel>(kSpritePanel, mAssets);
    mScriptEditor = mDock->addPanel<ScriptEditorPanel>(kScriptPanel);
    mSpriteEditor->addToScene.connect([this](const SpriteRegionDrag& region)
    {
        const rt::Rect area = mViewport->absoluteRect();
        addSprite(region.texture, region.id, region.rect, area.w * 0.5f, area.h * 0.5f);
    });
    mInspector = mDock->addPanel<InspectorPanel>(kInspectorPanel);
    mSettings = mDock->addPanel<SettingsPanel>(kSettingsPanel);
    mSettings->changed.connect([this](const PhysicsSettings& settings, bool project)
    {
        physicsChanged(settings, project);
    });
    mInspector->show(nullptr);
    mInspector->edited.connect([this](const char* label, bool final)
    {
        if (label)
            mPendingEdit = label;
        if (final && !mPendingEdit.empty())
            commit(mPendingEdit.c_str());
    });
    mInspector->addComponentRequested.connect([this](const char* type) { addComponent(type); });
    mInspector->removeComponentRequested.connect([this](Component* component)
    {
        if (mSelected && mSelected->removeComponent(component))
        {
            commit("Remove component");
            mInspectorStale = true;
        }
    });
    mConsole = mDock->addPanel<rt::ConsoleWidget>(kConsolePanel);
    mConsole->setMaxLines(2000);
    mConsole->setCollapseDuplicates(true);
    mConsole->onCommand.connect([this](const rt::String& command)
    {
        if (command == "clear")
            mConsole->clear();
    });
    log_set_callback(editorLog, nullptr);
    mAssetsPanel = mDock->addPanel<AssetsPanel>(kAssetsPanel, mAssets);
    mAnimator = mDock->addPanel<AnimatorPanel>(kAnimatorPanel, mAssets);
    mAnimator->spriteRegions = [this](ct::Vector<SpriteRegionDrag>& out) { return mSpriteEditor->allRegions(out); };
    mAnimator->edited.connect([this](const char* label) { commit(label); });
    mAnimator->addComponentRequested.connect([this](const char* type) { addComponent(type); });
    mParticles = mDock->addPanel<ParticlesPanel>(kParticlesPanel);
    mParticles->addComponentRequested.connect([this](const char* type) { addComponent(type); });
    mParticles->edited.connect([this](const char* label, bool final)
    {
        if (label)
            mPendingEdit = label;
        if (final && !mPendingEdit.empty())
            commit(mPendingEdit.c_str());
    });
    mProfiler = mDock->addPanel<ProfilerPanel>(kProfilerPanel);
    mTilePainter = mDock->addPanel<TilePainterPanel>(kTilePanel);
    mTilePainter->addComponentRequested.connect([this](const char* type) { addComponent(type); });
    mTilePainter->edited.connect([this](const char* label)
    {
        commit(label);
        mInspectorStale = true;
    });
    mAssetsPanel->fileOpened.connect([this](const ct::String& path)
    {
        const ct::String ext = EditorFileSystem::extension(path);
        if (ext == "k2dscene" && !mPlaying)
            openScene(EditorFileSystem::resolve(mProjectRoot, path).c_str());
        else if ((ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "tga") &&
                 mSpriteEditor->open(path))
            mDock->showPanel(kSpritePanel);
        else if ((ext == "py" || ext == "zen" || ext == "zs") && mScriptEditor->open(path))
            mDock->showPanel(kScriptPanel);
        else if (isPrefabPath(path) && !mPlaying)
        {
            const rt::Rect area = mViewport->absoluteRect();
            instantiatePrefab(path, nullptr, true, area.w * 0.5f, area.h * 0.5f);
        }
    });
    mDock->splitOff(kHierarchyPanel, rt::DockSide::Left, 0.2f);
    mDock->splitOff(kInspectorPanel, rt::DockSide::Right, 0.25f);
    mDock->splitOff(kConsolePanel, rt::DockSide::Bottom, 0.25f);
    mDock->moveTabToLeaf(kAssetsPanel, kConsolePanel);
    mDock->moveTabToLeaf(kAnimatorPanel, kConsolePanel);
    mDock->moveTabToLeaf(kTilePanel, kConsolePanel);
    mDock->moveTabToLeaf(kParticlesPanel, kConsolePanel);
    mDock->moveTabToLeaf(kProfilerPanel, kConsolePanel);
    mDock->moveTabToLeaf(kSettingsPanel, kInspectorPanel);
    mDock->showPanel(kInspectorPanel);
    mDock->showPanel(kAssetsPanel);
    mDock->showPanel(kScenePanel);
    mDefaultLayout = mDock->saveLayout();

    mViewport->panned.connect([this](float dx, float dy)
    {
        mCamera.position.x -= dx / mCamera.zoom.x;
        mCamera.position.y -= dy / mCamera.zoom.y;
    });
    mViewport->overlay = [this](rt::PaintContext& ctx, const rt::Rect& area)
    {
        mGizmo.setView(mCamera, area.w, area.h);
        mGizmo.paint(ctx, area, mScene, mSelected);
        mTilePainter->setView(mCamera, area.w, area.h);
        mTilePainter->paintOverlay(ctx, area);
    };
    mViewport->hovered.connect([this](float x, float y)
    {
        if (mTilePainter->painting())
        {
            const rt::Rect area = mViewport->absoluteRect();
            mTilePainter->setView(mCamera, area.w, area.h);
            mTilePainter->hover(x, y);
        }
    });
    mViewport->pressed.connect([this](float x, float y)
    {
        const rt::Rect area = mViewport->absoluteRect();
        mGizmo.setView(mCamera, area.w, area.h);
        if (mPlaying)
            return;
        if (mTilePainter->painting())
        {
            mTilePainter->setView(mCamera, area.w, area.h);
            mTilePainter->press(x, y);
            return;
        }
        if (mGizmo.press(mSelected, x, y))
            return;
        select(mGizmo.pick(mScene, x, y));
    });
    mViewport->acceptDrop = [this](const rt::DragPayload& payload)
    {
        return !mPlaying && (payload.type == kAssetPayload || payload.type == kSpriteRegionPayload);
    };
    mViewport->dropped = [this](const rt::DragPayload& payload, float x, float y)
    {
        if (const ct::String* path = std::any_cast<ct::String>(&payload.data))
            dropAsset(*path, x, y);
        else if (const SpriteRegionDrag* region = std::any_cast<SpriteRegionDrag>(&payload.data))
            addSprite(region->texture, region->id, region->rect, x, y);
    };
    mViewport->dragged.connect([this](float x, float y)
    {
        if (mTilePainter->painting())
            mTilePainter->drag(x, y);
        else
            mGizmo.drag(x, y);
    });
    mViewport->released.connect([this](float, float)
    {
        if (mTilePainter->painting())
        {
            if (mTilePainter->release())
                commit("Paint tiles");
            return;
        }
        if (mGizmo.release())
        {
            commit(mGizmo.editName());
            showObject(mSelected);
        }
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
    mMcpLabel = mStatus->createChild<rt::Label>("");
    app.setStage("main");
    app.unhandledKey.connect([this](rt::KeyEvent& e) { onShortcut(e); });
}

// Keys, buttons and the wheel go to the game while it plays in a visible Game
// view; the pointer always does, and the game reads it through its viewport.
void EditorWindow::onPlatformEvent(const Event& event)
{
    if (!mPlaying)
        return;
    const bool gameShown = mGame && mGame->isVisible();
    switch (event.type)
    {
    case EVENT_KEY:
        if (gameShown)
            mGameInput.OnKey(InputKeyFromPlatform(event.data.key.key), event.data.key.down, event.data.key.repeat);
        break;
    case EVENT_MOUSE_BUTTON:
        if (gameShown)
            mGameInput.OnMouseButton(InputButtonFromPlatform(event.data.mouse.button), event.data.mouse.down);
        break;
    case EVENT_MOUSE_MOVE:
        mGameInput.OnMouseMove(static_cast<float>(event.data.mouse.x), static_cast<float>(event.data.mouse.y));
        break;
    case EVENT_MOUSE_WHEEL:
        if (gameShown)
            mGameInput.OnWheel(event.data.wheel.y);
        break;
    default:
        break;
    }
}

void EditorWindow::setTool(SceneTool tool)
{
    mTools->toggleChanged.emit(mToolItems[static_cast<int>(tool)], true);
}

// Keys no widget took. Single keys are left alone while text is being typed.
void EditorWindow::onShortcut(rt::KeyEvent& e)
{
    const int key = e.key >= 'A' && e.key <= 'Z' ? e.key - 'A' + 'a' : e.key;
    if (e.ctrl)
    {
        // Ctrl+S saves what has the focus: the script being edited, or the scene.
        if (key == rt::Key::S && !e.shift && mScriptEditor->owns(mApp->focused()))
            mScriptEditor->save();
        else if (key == rt::Key::S)
            e.shift ? showSaveDialog() : save();
        else if (key == rt::Key::O)
            showOpenDialog();
        else if (key == rt::Key::N)
            newScene();
        else if (key == rt::Key::Z)
            e.shift ? redo() : undo();
        else if (key == rt::Key::Y)
            redo();
        else if (key == rt::Key::D)
            runHierarchyAction(HierarchyAction::Duplicate);
        else if (key == rt::Key::Q)
            window_set_should_close(mWindow, true);
        else
            return;
        e.consumed = true;
        return;
    }
    if (key == rt::Key::F1 + 4)   // F5: the function keys are consecutive
    {
        mPlaying ? stopPlay() : startPlay();
        e.consumed = true;
        return;
    }
    if (key == rt::Key::F1 + 5)   // F6
    {
        runInWindow();
        e.consumed = true;
        return;
    }
    rt::Widget* focused = mApp->focused();
    if (rt::widget_cast<rt::TextInput>(focused) || rt::widget_cast<rt::TextEdit>(focused))
        return;
    // The game playing in the Game view has the single keys.
    if (mPlaying && mGame->isVisible())
        return;
    if (key == rt::Key::Delete)
        runHierarchyAction(HierarchyAction::Delete);
    else if (key == rt::Key::Q)
        setTool(SceneTool::Select);
    else if (key == rt::Key::W)
        setTool(SceneTool::Move);
    else if (key == rt::Key::E)
        setTool(SceneTool::Rotate);
    else if (key == rt::Key::R)
        setTool(SceneTool::Scale);
    else if (key == rt::Key::F && mSelected)
        mCamera.position = mSelected->globalPosition();
    else
        return;
    e.consumed = true;
}

void EditorWindow::resetLayout()
{
    if (mDock)
        mDock->restoreLayout(mDefaultLayout);
}

void EditorWindow::registerTypes()
{
    static bool registered = false;
    if (registered)
        return;
    RegisterZenScriptSerializer();
    RegisterPhysics2DSerializers();
    RegisterCoreClasses();
    ClassDB::setAssets(&mAssets);
    SetZenScriptOutput(scriptOutput, nullptr);
    SetZenScriptInput(&mGameInput);
    SetUiInput(&mGameInput);
    registered = true;
}

bool EditorWindow::openScene(const char* path)
{
    registerTypes();
    const ct::String sceneDirectory = directoryOf(path);
    const ct::String projectRoot = directoryOf(sceneDirectory.c_str());
    // Absolute, so asset paths can be made relative to it.
    mProjectRoot = EditorFileSystem::isAbsolute(projectRoot)
                       ? projectRoot
                       : EditorFileSystem::join(EditorFileSystem::currentDirectory(), projectRoot.c_str());
    FileSystem& files = FileSystem::Instance();
    files.AddSearchPath(projectRoot.c_str());
    files.AddSearchPath(joinPath(projectRoot, "assets").c_str());
    files.AddSearchPath(sceneDirectory.c_str());

    stopPlay();
    mScene.clear();
    mScene.setSimulationEnabled(false);
    if (!GetSceneManager().Load(mScene, mAssets, path))
    {
        log_error("Could not open %s", path);
        return false;
    }
    log_info("Opened %s", path);
    mTopology = mScene.topologyVersion();
    // The editor camera starts where the game's does, so the scene opens in view.
    mCamera = Camera2D();
    if (CameraComponent* camera = mScene.activeCamera())
        mCamera.position = camera->owner()->globalPosition();
    mScenePath = EditorFileSystem::isAbsolute(path)
                     ? ct::String(path)
                     : EditorFileSystem::join(EditorFileSystem::currentDirectory(), path);
    FileBuffer text;
    mSceneDocument = files.LoadFile(path, text, true) ? ct::Json::parse(text.Text()) : ct::Json();
    loadPhysics();
    mAssetsPanel->setRoot(mProjectRoot);
    mSpriteEditor->setRoot(mProjectRoot);
    mScriptEditor->setRoot(mProjectRoot);
    sceneLoaded();
    return true;
}

void EditorWindow::sceneLoaded()
{
    restartEditPreview(mScene.root());
    mHierarchy->rebuild(mScene);
    mTopology = mScene.topologyVersion();
    select(nullptr);
    mCommands.clear();
    mCommitted = snapshot();
    mPendingEdit.clear();
    updateUndoButtons();
    markDirty(false);
}

// An empty scene with a camera, in the open project; it is saved with Save As.
void EditorWindow::newScene()
{
    registerTypes();
    stopPlay();
    mScene.clear();
    mScene.setSimulationEnabled(false);
    GameObject* cameraObject = mScene.createObject("Camera");
    if (Component* camera = Serializer::Create(*cameraObject, "Camera"))
        mScene.makeCameraActive(*static_cast<CameraComponent*>(camera));
    mCamera = Camera2D();
    mScenePath.clear();
    mSceneDocument = ct::Json();
    mPhysics.sceneGravity = false;
    applyPhysics();
    mSettings->show(mPhysics);
    sceneLoaded();
}

bool EditorWindow::saveScene(const char* path)
{
    if (!path || !path[0])
        return false;
    ct::Json document = mSceneDocument.is_object() ? mSceneDocument : ct::Json::object();
    // While playing, the scene as it was before Play is what gets saved.
    document.set("root", mPlaying ? mPlaySnapshot : snapshot());
    if (!FileSystem::Instance().SaveTextFile(path, document.dump(2)))
    {
        log_error("Could not save %s", path);
        return false;
    }
    mSceneDocument = document;
    mScenePath = path;
    markDirty(false);
    log_info("Saved %s", path);
    mAssetsPanel->refresh();
    return true;
}

void EditorWindow::save()
{
    if (mScenePath.empty())
        showSaveDialog();
    else
        saveScene(mScenePath.c_str());
}

void EditorWindow::markDirty(bool dirty)
{
    mDirty = dirty;
    ct::String text = mScenePath.empty() ? ct::String("Untitled scene") : EditorFileSystem::fileName(mScenePath);
    if (mDirty)
        text += " *";
    mSceneLabel->setText(text);
}

void EditorWindow::showOpenDialog()
{
    auto* dialog = mApp->addFloat<rt::FileDialog>("Open scene");
    dialog->setMode(rt::FileDialog::Mode::Open);
    dialog->setFilter("*.k2dscene");
    dialog->setPath(mProjectRoot.empty() ? EditorFileSystem::currentDirectory() : mProjectRoot);
    dialog->setRect({160.0f, 80.0f, 900.0f, 540.0f});
    dialog->accepted.connect([this](const rt::String& path)
    {
        if (!mPlaying)
            openScene(path.c_str());
    });
}

void EditorWindow::showSaveDialog()
{
    auto* dialog = mApp->addFloat<rt::FileDialog>("Save scene");
    dialog->setMode(rt::FileDialog::Mode::Save);
    dialog->setFilter("*.k2dscene");
    const ct::String scenes = EditorFileSystem::join(mProjectRoot, "scenes");
    dialog->setPath(!mScenePath.empty() ? directoryOf(mScenePath.c_str())
                    : EditorFileSystem::isDirectory(scenes) ? scenes
                    : (mProjectRoot.empty() ? EditorFileSystem::currentDirectory() : mProjectRoot));
    dialog->setFileName(mScenePath.empty() ? ct::String("scene.k2dscene") : EditorFileSystem::fileName(mScenePath));
    dialog->setRect({160.0f, 80.0f, 900.0f, 540.0f});
    dialog->accepted.connect([this](const rt::String& chosen)
    {
        ct::String path = chosen;
        if (EditorFileSystem::extension(path) != "k2dscene")
            path += ".k2dscene";
        saveScene(path.c_str());
    });
}

void EditorWindow::frame(float deltaTime, float pixelScale)
{
    Profiler::Get().beginFrame();
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
    mMcp.poll();
    if (mRunnerId > 0 && !process_is_running(mRunnerId))
    {
        const int code = process_exit_code(mRunnerId);
        if (code != 0)
            log_error("The game window closed with code %d", code);
        else
            log_info("The game window closed");
        mRunnerId = 0;
    }
    drainLog();
    mAnimator->tick(deltaTime);
    mParticles->tick();
    if (!mPlaying && mLivePreview)
        tickEditPreview(mScene.root(), deltaTime);

    if (mPlaying && !mPaused)
    {
        SetZenScriptFrameStats(deltaTime, mFpsTime > 0.0f ? mFpsFrames / mFpsTime : 0.0f);
        mScene.update(deltaTime);
        DispatchZenScriptEvents(mScene.root());
        ZenRuntime::instance().submitProfilerSamples();
        if (GetSceneManager().HasRequest() && GetSceneManager().ApplyRequest(mScene, mAssets))
            mScene.setSimulationEnabled(true);
    }
    // Scripts spawn and destroy objects while playing: the tree follows, and the
    // inspector lets go of an object that may be gone.
    if (mScene.topologyVersion() != mTopology)
    {
        mTopology = mScene.topologyVersion();
        mHierarchy->rebuild(mScene);
        select(nullptr);
    }
    if (mInspectorStale)
    {
        mInspectorStale = false;
        refreshTree();
        showObject(mSelected);
    }

    if (mViewport && mViewport->isVisible())
    {
        const rt::Rect area = mViewport->absoluteRect();
        renderScene(area.w, area.h, pixelScale);
    }
    if (mGame && mGame->isVisible())
        renderGame(mGame->absoluteRect(), pixelScale);
    // Pressed and released last one frame: the scene has read them by now.
    mGameInput.NewFrame();
    for (int key : mPendingReleases)
        mGameInput.OnKey(key, false, false);
    mPendingReleases.clear();
    Profiler::Get().endFrame();
    mProfiler->tick();
}

bool EditorWindow::ensureCanvas()
{
    if (!mCanvasReady && !mCanvasFailed)
    {
        mCanvasReady = mCanvas.Init();
        mCanvasFailed = !mCanvasReady;
    }
    return mCanvasReady;
}

void EditorWindow::renderScene(float width, float height, float pixelScale)
{
    const int pixelWidth = static_cast<int>(width * pixelScale + 0.5f);
    const int pixelHeight = static_cast<int>(height * pixelScale + 0.5f);
    if (!ensureCanvas() || !mTarget.resize(pixelWidth, pixelHeight))
        return;

    mTarget.bind();
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
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

// The scene as the game sees it: through the active camera, at the camera's
// resolution, letterboxed into the panel as the runner does in its window.
void EditorWindow::renderGame(const rt::Rect& area, float pixelScale)
{
    const int pixelWidth = static_cast<int>(area.w * pixelScale + 0.5f);
    const int pixelHeight = static_cast<int>(area.h * pixelScale + 0.5f);
    if (!ensureCanvas() || !mGameTarget.resize(pixelWidth, pixelHeight))
        return;

    CameraComponent* camera = mScene.activeCamera();
    const GameViewport viewport =
        camera ? CalculateGameViewport(static_cast<float>(pixelWidth), static_cast<float>(pixelHeight),
                                       camera->viewportWidth(), camera->viewportHeight(),
                                       camera->viewportScaleMode(), camera->integerScale())
               : CalculateGameViewport(static_cast<float>(pixelWidth), static_cast<float>(pixelHeight),
                                       area.w, area.h);
    if (!viewport.valid())
        return;

    // Scripts and UI read the pointer in window points, not framebuffer pixels.
    const float inverse = 1.0f / pixelScale;
    mGameViewX = area.x + viewport.x * inverse;
    mGameViewY = area.y + viewport.y * inverse;
    mGameViewW = viewport.width * inverse;
    mGameViewH = viewport.height * inverse;
    mGameVirtualW = viewport.virtualWidth;
    mGameVirtualH = viewport.virtualHeight;
    SetUiViewport(area.x + viewport.x * inverse, area.y + viewport.y * inverse, viewport.width * inverse,
                  viewport.height * inverse, viewport.virtualWidth, viewport.virtualHeight);
    SetZenScriptGameViewport(area.x + viewport.x * inverse, area.y + viewport.y * inverse,
                             viewport.width * inverse, viewport.height * inverse, viewport.virtualWidth,
                             viewport.virtualHeight);

    mGameTarget.bind();
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // The framebuffer is drawn flipped, so the letterbox is measured from the bottom.
    const float bottom = static_cast<float>(pixelHeight) - viewport.y - viewport.height;
    glViewport(static_cast<int>(viewport.x + 0.5f), static_cast<int>(bottom + 0.5f),
               static_cast<int>(viewport.width + 0.5f), static_cast<int>(viewport.height + 0.5f));
    if (camera)
    {
        camera->setRenderViewport(viewport.virtualWidth, viewport.virtualHeight);
        mCanvas.SetProjection(camera->projection());
        SetZenScriptGameCamera(&camera->camera());
        mScene.setRenderCamera(&camera->camera(), viewport.virtualWidth, viewport.virtualHeight);
    }
    else
    {
        mCanvas.SetProjection(mGameCamera.Projection(viewport.virtualWidth, viewport.virtualHeight));
        SetZenScriptGameCamera(&mGameCamera);
        mScene.setRenderCamera(&mGameCamera, viewport.virtualWidth, viewport.virtualHeight);
    }
    mScene.render(mCanvas);
    GetScreenFade().Draw(mCanvas, viewport.virtualWidth, viewport.virtualHeight);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    rt::TextureHandle texture;
    texture.value = mGameTarget.texture();
    mGame->setTexture(texture);
}

void EditorWindow::select(GameObject* object)
{
    mSelected = object;
    mHierarchy->select(object);
    showObject(object);
}

void EditorWindow::showObject(GameObject* object)
{
    mInspector->show(object);
    mAnimator->show(object);
    mParticles->show(object);
    mTilePainter->show(object);
}

// Shown again on the next frame: the request can come from a widget the
// inspector is about to replace.
void EditorWindow::addComponent(const char* type)
{
    if (mSelected && !mPlaying && Serializer::Create(*mSelected, type))
    {
        commit("Add component");
        mInspectorStale = true;
    }
}

void EditorWindow::refreshTree()
{
    mTopology = mScene.topologyVersion();
    mHierarchy->rebuild(mScene);
    mHierarchy->select(mSelected);
}

ct::Json EditorWindow::snapshot() const
{
    return Serializer::WriteObject(mScene.root(), const_cast<Assets*>(&mAssets));
}

void EditorWindow::restore(const ct::Json& state)
{
    const ct::Vector<std::size_t> path = pathOf(mSelected);
    select(nullptr);
    mScene.clear();
    Serializer::ReadInto(mScene, mScene.root(), state, &mAssets);
    mSelected = objectAt(mScene.root(), path);
    refreshTree();
    showObject(mSelected);
}

// Every edit is recorded as the whole scene before and after it, as the legacy
// editor did: simple, and right for any change the serializer can write.
void EditorWindow::commit(const char* label)
{
    mPendingEdit.clear();
    if (mPlaying)
        return;
    SceneCommand command;
    command.label = label;
    command.before = mCommitted;
    command.after = snapshot();
    mCommitted = command.after;
    mCommands.push(ct::detail::move(command));
    updateUndoButtons();
    markDirty(true);
}

void EditorWindow::undo()
{
    if (mPlaying)
        return;
    if (!mPendingEdit.empty())
        commit(mPendingEdit.c_str());
    if (const SceneCommand* command = mCommands.undo())
    {
        mCommitted = command->before;
        restore(mCommitted);
        markDirty(true);
    }
    updateUndoButtons();
}

void EditorWindow::redo()
{
    if (mPlaying)
        return;
    if (const SceneCommand* command = mCommands.redo())
    {
        mCommitted = command->after;
        restore(mCommitted);
        markDirty(true);
    }
    updateUndoButtons();
}

void EditorWindow::updateUndoButtons()
{
    mTools->setItemEnabled(mUndoItem, !mPlaying && mCommands.canUndo());
    mTools->setItemEnabled(mRedoItem, !mPlaying && mCommands.canRedo());
    char tip[128];
    std::snprintf(tip, sizeof tip, "Undo %s", mCommands.undoName());
    mTools->setItemTooltip(mUndoItem, tip);
    std::snprintf(tip, sizeof tip, "Redo %s", mCommands.redoName());
    mTools->setItemTooltip(mRedoItem, tip);
}

GameObject* EditorWindow::instantiatePrefab(const ct::String& path, GameObject* parent, bool placed, float x, float y)
{
    if (mPlaying)
        return nullptr;
    Prefab prefab;
    if (!prefab.Load(path.c_str()))
    {
        log_error("Could not load prefab %s", path.c_str());
        return nullptr;
    }
    GameObject* object = prefab.Instantiate(mScene, parent, &mAssets);
    if (!object)
    {
        log_error("Could not instantiate %s", path.c_str());
        return nullptr;
    }
    if (placed)
    {
        const rt::Rect area = mViewport->absoluteRect();
        const Math::Vec2 world = mCamera.ScreenToWorld(x, y, area.w, area.h);
        object->setPosition(object->parent()->globalTransform().AffineInverse().Transform(world));
    }
    mSelected = object;
    refreshTree();
    showObject(object);
    restartEditPreview(*object);
    commit("Add prefab");
    return object;
}

bool EditorWindow::savePrefab(GameObject* object, const ct::String& path)
{
    Prefab prefab;
    if (!object || !prefab.SaveToFile(path.c_str(), *object, &mAssets))
    {
        log_error("Could not save prefab %s", path.c_str());
        return false;
    }
    log_info("Saved prefab %s", path.c_str());
    mAssetsPanel->refresh();
    return true;
}

void EditorWindow::showSavePrefabDialog(GameObject* object)
{
    if (!object)
        return;
    auto* dialog = mApp->addFloat<rt::FileDialog>("Save as prefab");
    dialog->setMode(rt::FileDialog::Mode::Save);
    dialog->setFilter("*.k2dprefab");
    const ct::String prefabs = EditorFileSystem::join(mProjectRoot, "assets/prefabs");
    dialog->setPath(EditorFileSystem::isDirectory(prefabs) ? prefabs
                    : (mProjectRoot.empty() ? EditorFileSystem::currentDirectory() : mProjectRoot));
    dialog->setFileName(object->name() + ".k2dprefab");
    dialog->setRect({160.0f, 80.0f, 900.0f, 540.0f});
    // The object is looked up again by path: the scene may change meanwhile.
    const ct::Vector<std::size_t> where = pathOf(object);
    dialog->accepted.connect([this, where](const rt::String& chosen)
    {
        ct::String path = chosen;
        if (EditorFileSystem::extension(path) != "k2dprefab")
            path += ".k2dprefab";
        savePrefab(objectAt(mScene.root(), where), path);
    });
}

void EditorWindow::attachScript(const ct::String& path, GameObject* object, float x, float y)
{
    if (mPlaying)
        return;
    if (!object)
    {
        const rt::Rect area = mViewport->absoluteRect();
        object = mScene.createObject(EditorFileSystem::withoutExtension(EditorFileSystem::fileName(path)).c_str());
        object->setPosition(mCamera.ScreenToWorld(x, y, area.w, area.h));
    }
    ZenScriptComponent* script = object->addComponent<ZenScriptComponent>();
    if (!script->loadFile(path.c_str()))
        log_error("Could not load %s", path.c_str());
    else
        log_info("Added %s to %s", path.c_str(), object->name().c_str());
    mSelected = object;
    refreshTree();
    showObject(object);
    commit("Add script");
}

// An image dropped on the scene view becomes a sprite object where it lands; a
// script goes onto the object under the pointer, or a new one there.
void EditorWindow::dropAsset(const ct::String& path, float x, float y)
{
    if (isScriptPath(path))
    {
        const rt::Rect area = mViewport->absoluteRect();
        mGizmo.setView(mCamera, area.w, area.h);
        attachScript(path, mGizmo.pick(mScene, x, y), x, y);
        return;
    }
    if (isPrefabPath(path))
    {
        instantiatePrefab(path, nullptr, true, x, y);
        return;
    }
    if (!isImagePath(path))
        return;
    const ct::String name = EditorFileSystem::withoutExtension(EditorFileSystem::fileName(path));
    addSprite(path, name, Math::Vec4(0.0f), x, y);
}

void EditorWindow::addSprite(const ct::String& path, const ct::String& name, const Math::Vec4& rect, float x, float y)
{
    Texture* texture = mAssets.GetTexture(path.c_str());
    if (!texture || mPlaying)
        return;
    const rt::Rect area = mViewport->absoluteRect();
    const Math::Vec2 world = mCamera.ScreenToWorld(x, y, area.w, area.h);
    GameObject* object = mScene.createObject(name.c_str());
    object->setPosition(world);
    SpriteComponent* sprite = object->addComponent<SpriteComponent>();
    sprite->setTexture(texture);
    if (rect.z > 0.0f && rect.w > 0.0f)
    {
        sprite->setSourceRect(rect.x, rect.y, rect.z, rect.w);
        sprite->setSize(Math::Vec2(rect.z, rect.w));
    }
    else
        sprite->setSize(Math::Vec2(static_cast<float>(texture->Width()), static_cast<float>(texture->Height())));
    mSelected = object;
    refreshTree();
    showObject(object);
    commit("Add sprite");
}

// Inside makes the object the last child of the target; Before and After put it
// next to the target, under the target's parent. The local transform is kept.
void EditorWindow::dropObject(GameObject* object, GameObject* target, rt::TreeView::DropZone zone)
{
    if (mPlaying || !object)
        return;
    GameObject* parent = &mScene.root();
    std::size_t index = 0;
    if (!target)
        index = parent->childCount();
    else if (zone == rt::TreeView::DropZone::Inside)
    {
        parent = target;
        index = target->childCount();
    }
    else
    {
        parent = target->parent();
        index = parent->childIndex(target) + (zone == rt::TreeView::DropZone::After ? 1 : 0);
    }

    if (object->parent() == parent)
    {
        // The object leaves its slot first, so a slot further down moves up one.
        if (parent->childIndex(object) < index)
            --index;
    }
    else if (!mScene.reparent(object, parent))
        return;
    // A reparented object arrives last; walk it up (or down) to its slot.
    std::size_t current = parent->childIndex(object);
    while (current > index && parent->moveChildUp(object))
        --current;
    while (current < index && parent->moveChildDown(object))
        ++current;

    mSelected = object;
    refreshTree();
    showObject(object);
    commit("Reparent");
}

void EditorWindow::runHierarchyAction(HierarchyAction action)
{
    if (mPlaying)
        return;
    GameObject* selected = mSelected;
    switch (action)
    {
    case HierarchyAction::Create:
    {
        GameObject* object = mScene.createObject("GameObject", selected ? selected->parent() : nullptr);
        mSelected = object;
        refreshTree();
        showObject(object);
        commit("New object");
        break;
    }
    case HierarchyAction::Duplicate:
    {
        if (!selected)
            return;
        const ct::Json copy = Serializer::WriteObject(*selected, &mAssets);
        GameObject* object = Serializer::ReadObject(mScene, copy, selected->parent(), &mAssets);
        if (!object)
            return;
        mSelected = object;
        refreshTree();
        showObject(object);
        commit("Duplicate");
        break;
    }
    case HierarchyAction::MoveUp:
    case HierarchyAction::MoveDown:
    {
        GameObject* parent = selected ? selected->parent() : nullptr;
        if (!parent)
            return;
        const bool moved = action == HierarchyAction::MoveUp ? parent->moveChildUp(selected)
                                                             : parent->moveChildDown(selected);
        if (!moved)
            return;
        refreshTree();
        commit(action == HierarchyAction::MoveUp ? "Move up" : "Move down");
        break;
    }
    case HierarchyAction::SavePrefab:
        showSavePrefabDialog(selected);
        break;
    case HierarchyAction::Delete:
    {
        if (!selected)
            return;
        // Objects in a scene are only disposed of during an update, which the
        // editor does not run: the object is cut out of a snapshot instead.
        const ct::Vector<std::size_t> path = pathOf(selected);
        ct::Json state = snapshot();
        ct::Json* children = &state;
        for (std::size_t i = 0; i + 1 < path.size(); ++i)
            children = &(*children)["children"][path[i]];
        (*children)["children"].erase(path.back());
        mSelected = nullptr;
        restore(state);
        commit("Delete");
        break;
    }
    }
}

void EditorWindow::startPlay()
{
    if (mPlaying)
        return;
    // Play simulates the scene in place, as the legacy editor did; Stop throws
    // away what physics and scripts changed by reading the snapshot back.
    mPlaySnapshot = Serializer::WriteObject(mScene.root(), &mAssets);
    GetSceneManager().ClearRequest();
    SetZenScriptAssets(&mAssets);
    RouteZenScriptCollisions(mScene);
    RouteZenScriptAnimationEvents(mScene);
    RouteZenScriptActionEvents(mScene);
    RouteZenScriptParticleHits(mScene);
    SetZenScriptsEnabled(true);
    mScene.setSimulationEnabled(true);
    mPlaying = true;
    mScriptEditor->playing = true;
    mPaused = false;
    mDock->showPanel(kGamePanel);
    updateRunBar();
    updateUndoButtons();
}

void EditorWindow::stopPlay()
{
    if (!mPlaying)
        return;
    SetZenScriptsEnabled(false);
    GetSceneManager().ClearRequest();
    select(nullptr);
    mScene.clear();
    mScene.setSimulationEnabled(false);
    applyPhysics();
    Serializer::ReadInto(mScene, mScene.root(), mPlaySnapshot, &mAssets);
    mPlaySnapshot = ct::Json();
    mPlaying = false;
    mScriptEditor->playing = false;
    restartEditPreview(mScene.root());
    mPaused = false;
    mTopology = mScene.topologyVersion();
    mHierarchy->rebuild(mScene);
    mSelected = nullptr;
    updateRunBar();
    updateUndoButtons();
}

void EditorWindow::runInWindow()
{
    if (mScenePath.empty())
    {
        log_warn("Save the scene once before running it in a window");
        showSaveDialog();
        return;
    }
    if (mDirty && !saveScene(mScenePath.c_str()))
        return;
    // One game window at a time: run again means start over.
    if (mRunnerId > 0 && process_is_running(mRunnerId))
        process_kill(mRunnerId);
    char base[1024];
    if (!fs_get_base_path(base, sizeof base))
        base[0] = '\0';
#if defined(_WIN32)
    const ct::String runner = ct::String(base) + "k2d_runner.exe";
#else
    const ct::String runner = ct::String(base) + "k2d_runner";
#endif
    if (!EditorFileSystem::exists(runner))
    {
        log_error("No runner at %s: build k2d_runner", runner.c_str());
        return;
    }
    const ct::String project = EditorFileSystem::join(mProjectRoot, "project.k2dproj");
    const char* args[2] = {mScenePath.c_str(), project.c_str()};
    mRunnerId = process_spawn(runner.c_str(), args, EditorFileSystem::exists(project) ? 2 : 1, false);
    if (mRunnerId > 0)
        log_info("Running %s in a window", EditorFileSystem::fileName(mScenePath).c_str());
    else
        log_error("Could not start the runner");
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

void EditorWindow::tickEditPreview(GameObject& object, float deltaTime)
{
    if (!object.isActiveInHierarchy())
        return;
    for (std::size_t i = 0; i < object.componentCount<ParticleComponent>(); ++i)
    {
        ParticleComponent* particles = object.getComponentAt<ParticleComponent>(i);
        if (!particles || !particles->active())
            continue;
        if (particles->followOwner())
            particles->system().SetEmitterPosition(object.globalPosition());
        particles->system().Update(deltaTime);
    }
    for (std::size_t i = 0; i < object.componentCount<Animation2D>(); ++i)
        if (Animation2D* animation = object.getComponentAt<Animation2D>(i); animation && animation->active())
            animation->Advance(deltaTime);
    for (std::size_t i = 0; i < object.componentCount<ActionSequence2D>(); ++i)
        if (ActionSequence2D* sequence = object.getComponentAt<ActionSequence2D>(i);
            sequence && sequence->active() && sequence->playing())
            sequence->Advance(deltaTime);
    for (std::size_t i = 0; i < object.childCount(); ++i)
        tickEditPreview(*object.child(i), deltaTime);
}

void EditorWindow::restartEditPreview(GameObject& object)
{
    for (std::size_t i = 0; i < object.componentCount<ParticleComponent>(); ++i)
        if (ParticleComponent* particles = object.getComponentAt<ParticleComponent>(i))
            particles->system().Reset();
    for (std::size_t i = 0; i < object.componentCount<Animation2D>(); ++i)
        if (Animation2D* animation = object.getComponentAt<Animation2D>(i))
            animation->reset();
    for (std::size_t i = 0; i < object.componentCount<ActionSequence2D>(); ++i)
        if (ActionSequence2D* sequence = object.getComponentAt<ActionSequence2D>(i))
        {
            if (sequence->autoplay())
                sequence->play(true);
            else
                sequence->stop();
        }
    for (std::size_t i = 0; i < object.childCount(); ++i)
        restartEditPreview(*object.child(i));
}

// As the runner does: the project's physics, the scene's gravity over it.
void EditorWindow::loadPhysics()
{
    mPhysics = PhysicsSettings();
    mProjectDocument = ct::Json();
    FileBuffer file;
    const ct::String projectFile = EditorFileSystem::join(mProjectRoot, "project.k2dproj");
    if (EditorFileSystem::exists(projectFile) && FileSystem::Instance().LoadFile(projectFile.c_str(), file, true))
        mProjectDocument = ct::Json::parse(file.Text());
    auto readVec = [](const ct::Json& json, const Math::Vec2& fallback) {
        return json.is_array() && json.size() >= 2 ? Math::Vec2(static_cast<float>(json[0].as_double(fallback.x)),
                                                                static_cast<float>(json[1].as_double(fallback.y)))
                                                   : fallback;
    };
    const ct::Json& physics = mProjectDocument["physics"];
    if (physics.is_object())
    {
        mPhysics.gravity = readVec(physics["gravity"], mPhysics.gravity);
        mPhysics.fixedTimeStep = static_cast<float>(physics["fixedTimeStep"].as_double(mPhysics.fixedTimeStep));
        mPhysics.velocityIterations = static_cast<int>(physics["velocityIterations"].as_int(mPhysics.velocityIterations));
        mPhysics.treeBroadphase = physics["treeBroadphase"].as_bool(mPhysics.treeBroadphase);
    }
    const ct::Json& scenePhysics = mSceneDocument["physics"];
    mPhysics.sceneGravity = scenePhysics.is_object() && scenePhysics["gravity"].is_array();
    mPhysics.sceneGravityValue = readVec(scenePhysics["gravity"], mPhysics.gravity);
    applyPhysics();
    mSettings->show(mPhysics);
}

void EditorWindow::applyPhysics()
{
    mScene.setGravity(mPhysics.effectiveGravity());
    mScene.setFixedTimeStep(mPhysics.fixedTimeStep);
    mScene.setVelocityIterations(mPhysics.velocityIterations);
    mScene.setTreeBroadphase(mPhysics.treeBroadphase);
}

void EditorWindow::physicsChanged(const PhysicsSettings& settings, bool project)
{
    mPhysics = settings;
    applyPhysics();
    auto vec = [](const Math::Vec2& v) {
        ct::Json a = ct::Json::array();
        a.push_back(ct::Json(static_cast<double>(v.x)));
        a.push_back(ct::Json(static_cast<double>(v.y)));
        return a;
    };
    if (project)
    {
        if (mProjectRoot.empty())
            return;
        if (!mProjectDocument.is_object())
            mProjectDocument = ct::Json::object();
        ct::Json physics = ct::Json::object();
        physics.set("gravity", vec(mPhysics.gravity));
        physics.set("fixedTimeStep", ct::Json(static_cast<double>(mPhysics.fixedTimeStep)));
        physics.set("velocityIterations", ct::Json(mPhysics.velocityIterations));
        physics.set("treeBroadphase", ct::Json(mPhysics.treeBroadphase));
        mProjectDocument.set("physics", physics);
        const ct::String projectFile = EditorFileSystem::join(mProjectRoot, "project.k2dproj");
        if (FileSystem::Instance().SaveTextFile(projectFile.c_str(), mProjectDocument.dump(2)))
            log_info("Saved project physics");
        else
            log_error("Could not save %s", projectFile.c_str());
        return;
    }
    // The scene's gravity is written with the scene.
    if (!mSceneDocument.is_object())
        mSceneDocument = ct::Json::object();
    if (mPhysics.sceneGravity)
    {
        ct::Json physics = mSceneDocument["physics"].is_object() ? mSceneDocument["physics"] : ct::Json::object();
        physics.set("gravity", vec(mPhysics.sceneGravityValue));
        mSceneDocument.set("physics", physics);
    }
    else if (mSceneDocument.contains("physics"))
    {
        ct::Json physics = mSceneDocument["physics"];
        physics.erase("gravity");
        if (physics.size() == 0)
            mSceneDocument.erase("physics");
        else
            mSceneDocument.set("physics", physics);
    }
    markDirty(true);
}

void EditorWindow::drainLog()
{
    ct::Vector<PendingLine> lines;
    {
        std::lock_guard<std::mutex> lock(gLogMutex);
        if (gLogLines.empty())
            return;
        lines = ct::detail::move(gLogLines);
        gLogLines.clear();
    }
    static const char* const levels[] = {"trace", "info", "warn", "error"};
    for (const PendingLine& line : lines)
    {
        mConsole->log(line.level, line.text);
        mLogHistory.push_back(ct::String(levels[static_cast<int>(line.level)]) + ": " + line.text);
    }
    const std::size_t keep = 1000;
    if (mLogHistory.size() > keep * 2)
    {
        const std::size_t drop = mLogHistory.size() - keep;
        mLogHistory.erase(mLogHistory.begin(), mLogHistory.begin() + drop);
        mLogFirst += drop;
    }
}

bool EditorWindow::setMcp(std::uint16_t port)
{
    if (port == 0)
        mMcp.stop();
    else
        mMcp.start(port);
    char text[64];
    std::snprintf(text, sizeof text, "MCP 127.0.0.1:%u", static_cast<unsigned>(mMcp.port()));
    mMcpLabel->setText(mMcp.running() ? text : "");
    if (mMcpAction)
        mMcpAction->setChecked(mMcp.running());
    return mMcp.running();
}

void EditorWindow::shutdown()
{
    if (mRunnerId > 0 && process_is_running(mRunnerId))
        process_kill(mRunnerId);
    mMcp.stop();
    log_set_callback(nullptr, nullptr);
    stopPlay();
    mTarget.destroy();
    mGameTarget.destroy();
    if (mCanvasReady)
        mCanvas.Shutdown();
    mCanvasReady = false;
}

} // namespace k2d::editor
