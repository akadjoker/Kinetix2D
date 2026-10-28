#include "EditorWindow.h"

#include "Icons.h"

#include <k2d/ClassDB.h>
#include <k2d/FileSystem.h>
#include <k2d/CameraComponent.h>
#include <k2d/GameObject.h>
#include <k2d/GameViewport.h>
#include <k2d/OpenGL.h>
#include <k2d/Physics2DSerializer.h>
#include <k2d/SceneManager.h>
#include <k2d/ScreenFade.h>
#include <k2d/UiControls.h>
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
const char* const kGamePanel = "Game";
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
    rt::Menu* edit = menuBar->addMenu("Edit");
    edit->addAction("Undo", [this]() { undo(); })->setShortcut("Ctrl+Z");
    edit->addAction("Redo", [this]() { redo(); })->setShortcut("Ctrl+Y");
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
    updateUndoButtons();

    mDock = outer->createChild<rt::DockPanel>();
    mDock->setStretch(1.0f);
    mHierarchy = mDock->addPanel<HierarchyPanel>(kHierarchyPanel);
    mHierarchy->selectionChanged.connect([this](GameObject* object)
    {
        mSelected = object;
        mInspector->show(object);
    });
    mHierarchy->actionRequested.connect([this](HierarchyAction action) { runHierarchyAction(action); });
    mHierarchy->edited.connect([this](const char* label) { commit(label); });
    mHierarchy->dropped.connect([this](GameObject* object, GameObject* target, rt::TreeView::DropZone zone)
    {
        dropObject(object, target, zone);
    });
    mViewport = mDock->addPanel<ViewportPanel>(kScenePanel);
    mGame = mDock->addPanel<ViewportPanel>(kGamePanel);
    mInspector = mDock->addPanel<InspectorPanel>(kInspectorPanel);
    mInspector->show(nullptr);
    mInspector->edited.connect([this](const char* label, bool final)
    {
        if (label)
            mPendingEdit = label;
        if (final && !mPendingEdit.empty())
            commit(mPendingEdit.c_str());
    });
    mInspector->addComponentRequested.connect([this](const char* type)
    {
        if (mSelected && Serializer::Create(*mSelected, type))
        {
            commit("Add component");
            mInspectorStale = true;
        }
    });
    mInspector->removeComponentRequested.connect([this](Component* component)
    {
        if (mSelected && mSelected->removeComponent(component))
        {
            commit("Remove component");
            mInspectorStale = true;
        }
    });
    mDock->addPanel<rt::Label>(kConsolePanel, "");
    mDock->splitOff(kHierarchyPanel, rt::DockSide::Left, 0.2f);
    mDock->splitOff(kInspectorPanel, rt::DockSide::Right, 0.25f);
    mDock->splitOff(kConsolePanel, rt::DockSide::Bottom, 0.25f);
    mDock->showPanel(kScenePanel);
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
    select(nullptr);
    mCommands.clear();
    mCommitted = snapshot();
    mPendingEdit.clear();
    updateUndoButtons();
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
        select(nullptr);
    }
    if (mInspectorStale)
    {
        mInspectorStale = false;
        refreshTree();
        mInspector->show(mSelected);
    }

    if (mViewport && mViewport->isVisible())
    {
        const rt::Rect area = mViewport->absoluteRect();
        renderScene(area.w, area.h, pixelScale);
    }
    if (mGame && mGame->isVisible())
        renderGame(mGame->absoluteRect(), pixelScale);
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
    mInspector->show(object);
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
    mInspector->show(mSelected);
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
    mInspector->show(object);
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
        mInspector->show(object);
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
        mInspector->show(object);
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
    mScene.setGravity(mEditGravity);
    Serializer::ReadInto(mScene, mScene.root(), mPlaySnapshot, &mAssets);
    mPlaySnapshot = ct::Json();
    mPlaying = false;
    mPaused = false;
    mTopology = mScene.topologyVersion();
    mHierarchy->rebuild(mScene);
    mSelected = nullptr;
    updateRunBar();
    updateUndoButtons();
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
    mGameTarget.destroy();
    if (mCanvasReady)
        mCanvas.Shutdown();
    mCanvasReady = false;
}

} // namespace k2d::editor
