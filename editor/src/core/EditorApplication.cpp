#include "EditorApplication.h"

#include "EditorFileSystem.h"
#include "EditorPanel.h"
#include "EditorTheme.h"
#include "panels/AssetsPanel.h"
#include "panels/AnimatorPanel.h"
#include "panels/AtlasPanel.h"
#include "panels/ConsolePanel.h"
#include "panels/GamePanel.h"
#include "panels/HierarchyPanel.h"
#include "panels/ImageEditorPanel.h"
#include "panels/InspectorPanel.h"
#include "panels/ParticlePanel.h"
#include "panels/PrefabsPanel.h"
#include "panels/ScriptsPanel.h"
#include "panels/SettingsPanel.h"
#include "panels/SceneViewportPanel.h"
#include "panels/SkeletonPanel.h"
#include "panels/ScriptEditorPanel.h"
#include "panels/SpriteEditorPanel.h"
#include "panels/TileMapPanel.h"
#include "widgets/EditorToolbar.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <IconsMaterialDesignIcons.h>
#include <k2d/ActionSequence2D.h>
#include <k2d/Animation2D.h>
#include <k2d/AudioEngine.h>
#include <k2d/CameraComponent.h>
#include <k2d/MouseCursor.h>
#include <k2d/BoxCollider2D.h>
#include <k2d/CircleCollider2D.h>
#include <k2d/CircleShape.h>
#include <k2d/FileBuffer.h>
#include <k2d/FileSystem.h>
#include <k2d/InputActionMap.h>
#include <k2d/ParticleComponent.h>
#include <k2d/Prefab.h>
#include <k2d/Profiler.h>
#include <k2d/ProfilerUI.h>
#include <k2d/RectShape.h>
#include <k2d/ScreenFade.h>
#include <k2d/SpriteComponent.h>
#include <k2d/RigidBody2D.h>
#include <k2d/Serializer.h>
#include <k2d/Physics2DSerializer.h>
#include <k2d/ZenScriptComponent.h>
#include <k2d/ZenRuntime.h>
#include <k2d/UiControls.h>
#include <k2d/UiTheme.h>


#include <SDL.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

namespace k2d::editor
{

namespace
{
Math::Vec2 readVec2(const ct::Json& value, const Math::Vec2& fallback)
{
    if (!value.is_array() || value.size() < 2)
        return fallback;
    return Math::Vec2(static_cast<float>(value[0].as_double(fallback.x)),
                      static_cast<float>(value[1].as_double(fallback.y)));
}

GameObject* findById(GameObject& object, uint64_t id)
{
    if (object.id() == id)
        return &object;
    for (size_t i = 0; i < object.childCount(); ++i)
        if (GameObject* found = findById(*object.child(i), id))
            return found;
    return nullptr;
}

void configureDefaultInputActions()
{
    InputActionMap& actions = GetInputActions();
    actions.Clear();
    actions.Bind("move_forward", SDL_SCANCODE_W);
    actions.Bind("move_forward", SDL_SCANCODE_UP);
    actions.Bind("move_backward", SDL_SCANCODE_S);
    actions.Bind("move_backward", SDL_SCANCODE_DOWN);
    actions.Bind("turn_left", SDL_SCANCODE_A);
    actions.Bind("turn_left", SDL_SCANCODE_LEFT);
    actions.Bind("turn_right", SDL_SCANCODE_D);
    actions.Bind("turn_right", SDL_SCANCODE_RIGHT);
    actions.Bind("primary", SDL_SCANCODE_SPACE);
    actions.Bind("secondary", SDL_SCANCODE_LCTRL);
}

ct::String relativeToRoot(const ct::String& absolute, const ct::String& root)
{
    if (root.empty() || absolute.size() <= root.size())
        return ct::String();
    for (size_t i = 0; i < root.size(); ++i)
        if (absolute[i] != root[i])
            return ct::String();
    size_t start = root.size();
    if (absolute[start] != '/' && absolute[start] != '\\')
        return ct::String();
    ++start;
    return absolute.substr(start, absolute.size() - start);
}

std::filesystem::path findEditorTool(const char* name, const char* overrideVariable = nullptr)
{
    if (overrideVariable)
        if (const char* configured = std::getenv(overrideVariable))
            if (configured[0] && std::filesystem::exists(configured))
                return std::filesystem::path(configured);

    std::filesystem::path directory = FileSystem::Instance().BasePath();
    while (!directory.empty())
    {
        const std::filesystem::path besideExecutable = directory / name;
        if (std::filesystem::exists(besideExecutable))
            return besideExecutable;

        const std::filesystem::path sourceTool = directory / "tools" / name;
        if (std::filesystem::exists(sourceTool))
            return sourceTool;

        const std::filesystem::path parent = directory.parent_path();
        if (parent == directory)
            break;
        directory = parent;
    }
    return std::filesystem::path();
}

#if defined(_WIN32)
std::string quoteWindowsArgument(const std::string& value)
{
    std::string quoted("\"");
    for (const char c : value)
    {
        if (c == '\"')
            quoted += "\\\"";
        else
            quoted += c;
    }
    quoted += "\"";
    return quoted;
}
#endif
} // namespace

EditorApplication::~EditorApplication() = default;

bool EditorApplication::initialize()
{
    if (!mDevice.Init("Kinetix2D Editor", 1600, 900, true))
        return false;
    if (!mUserData.open("Kinetix2D", "Editor"))
        log("Could not open the persistent editor data folder");
    else
        mUserData.load();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = "k2d_editor.ini";
    io.Fonts->AddFontDefault();
    if (!loadEditorIconFont(io))
        log("Material Design icon font failed to load");
    applyEditorTheme();

    RegisterZenScriptSerializer();
    RegisterPhysics2DSerializers();
    configureDefaultInputActions();
    SetZenScriptInput(&mDevice.GetInput());
    SetUiInput(&mDevice.GetInput());
    if (!GetAudio().Init())
        log("Audio unavailable; preview will run without sound");
    else
        GetAudio().LoadSettings(mUserData);
    SetUiThemeTexture(UiTheme::DefaultTexture(mAssets));
    SetZenScriptAssets(&mAssets);
    SetZenScriptUserData(&mUserData);
    SetZenScriptOutput(
        [](const char* text, bool isError, void* user)
        {
            EditorApplication& self = *static_cast<EditorApplication*>(user);
            static ct::String lineBuffer;
            for (const char* c = text; *c; ++c)
            {
                if (*c == '\n')
                {
                    ct::String message(isError ? "[script] ERROR: " : "[script] ");
                    message += lineBuffer;
                    self.log(message);
                    lineBuffer.clear();
                }
                else
                {
                    lineBuffer.push_back(*c);
                }
            }
        },
        this);

    loadSettings();
    mDevice.SetDisplayIndex(mSettings.windowDisplayIndex);
    createPanels();
    particlePlaceholderTexture();
    bool opened = false;
    if (!mSettings.lastProjectPath.empty() && mProject.load(mSettings.lastProjectPath.c_str()))
    {
        FileSystem::Instance().ResetSearchPaths();
        FileSystem::Instance().AddSearchPath(mProject.root().c_str());
        ct::String message("Project restored: ");
        message += mProject.root();
        log(message);
        if (!mSettings.lastScenePath.empty())
            opened = openScene(mSettings.lastScenePath.c_str());
        if (!opened && !mProject.startupScene().empty())
            opened = openScene(EditorFileSystem::join(mProject.root(), mProject.startupScene().c_str()).c_str());
    }
    else if (!mSettings.lastScenePath.empty())
    {
        opened = openScene(mSettings.lastScenePath.c_str());
    }
    if (!opened)
        newScene();
    log("Kinetix2D Editor initialized");
    log("Dear ImGui is shared with the engine; editor widgets loaded from editor/external");
    mInitialized = true;
    return true;
}

int EditorApplication::run()
{
    if (!mInitialized)
        return 1;

    bool running = true;
    while (running)
    {
        Profiler::Get().beginFrame();
        running = mDevice.PollEvents();
        GetAudio().Update();

        if (mPlaying && !mPaused)
        {
            SetZenScriptFrameStats(mDevice.DeltaTime(), mDevice.FPS());
            GetScreenFade().Update(mDevice.DeltaTime());
            mScene.update(mDevice.DeltaTime());
            if (CameraComponent* camera = mScene.activeCamera())
                GetAudio().SetListenerPosition(camera->camera().position);
            DispatchZenScriptEvents(mScene.root());
        }
        if (!mPlaying && mSettings.viewportLivePreview)
            tickEditPreview(mScene.root(), mDevice.DeltaTime());

        glClearColor(0.055f, 0.062f, 0.075f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        mDevice.BeginUI();
        handleShortcuts();
        drawWorkspace();
        for (const ct::Unique<EditorPanel>& panel : mPanels)
            panel->draw();
        ShowProfilerWindow(&mProfilerOpen);
        if (mDefaultFocusPending)
        {
            ImGui::SetWindowFocus("Hierarchy");
            ImGui::SetWindowFocus("Scene");
            mDefaultFocusPending = false;
        }
        drawStatusBar();
        mToasts.update(mDevice.DeltaTime());
        mToasts.draw();
        mDevice.EndUI();
        ZenRuntime::instance().setVmProfiling(mProfilerOpen);
        ZenRuntime::instance().submitProfilerSamples();
        if (mCaptureScreenshotPending)
        {
            mDevice.CaptureScreenshot();
            mCaptureScreenshotPending = false;
        }
        if (mDevice.IsGifCapturing())
            mDevice.CaptureGifFrame();
        mDevice.Swap();
        Profiler::Get().endFrame();
    }
    return 0;
}

void EditorApplication::shutdown()
{
    if (!mInitialized)
        return;
    mSettings.windowDisplayIndex = mDevice.DisplayIndex();
    saveSettings();
    GetAudio().SaveSettings(mUserData);
    mUserData.save();
    if (mDevice.IsGifCapturing())
        mDevice.StopGifCapture();
    mPanels.clear();
    mSelection.clear();
    SetZenScriptUserData(nullptr);
    GetAudio().Shutdown();
    mDevice.Shutdown();
    mInitialized = false;
}

void EditorApplication::loadSettings()
{
    const ct::String path = mUserData.filePath("editor_settings.json");
    if (path.empty() || !mSettings.load(path.c_str()))
        log("No saved editor settings found - using defaults");
}

void EditorApplication::saveSettings()
{
    const ct::String path = mUserData.filePath("editor_settings.json");
    if (path.empty() || !mSettings.save(path.c_str()))
        log("Could not write editor settings");
}

void EditorApplication::createPanels()
{
    mPanels.push_back(ct::make_unique<HierarchyPanel>(*this));
    mPanels.push_back(ct::make_unique<SettingsPanel>(*this));
    mPanels.push_back(ct::make_unique<InspectorPanel>(*this));
    ct::Unique<SceneViewportPanel> sceneViewport = ct::make_unique<SceneViewportPanel>(*this);
    mSceneViewport = sceneViewport.get();
    mPanels.push_back(ct::detail::move(sceneViewport));
    mPanels.push_back(ct::make_unique<GamePanel>(*this));
    mPanels.push_back(ct::make_unique<ParticlePanel>(*this));
    mPanels.push_back(ct::make_unique<TileMapPanel>(*this));
    mPanels.push_back(ct::make_unique<SpriteEditorPanel>(*this));
    ct::Unique<ImageEditorPanel> imageEditor = ct::make_unique<ImageEditorPanel>(*this);
    mImageEditor = imageEditor.get();
    mPanels.push_back(ct::detail::move(imageEditor));
    mPanels.push_back(ct::make_unique<AnimatorPanel>(*this));
    mPanels.push_back(ct::make_unique<SkeletonPanel>(*this));
    mPanels.push_back(ct::make_unique<AssetsPanel>(*this));
    mPanels.push_back(ct::make_unique<AtlasPanel>(*this));
    mPanels.push_back(ct::make_unique<PrefabsPanel>(*this));
    mPanels.push_back(ct::make_unique<ScriptsPanel>(*this));
    ct::Unique<ScriptEditorPanel> scriptEditor = ct::make_unique<ScriptEditorPanel>(*this);
    mScriptEditor = scriptEditor.get();
    mPanels.push_back(ct::detail::move(scriptEditor));
    mPanels.push_back(ct::make_unique<ConsolePanel>(*this));
}

void EditorApplication::openScriptEditor(const char* path)
{
    if (mScriptEditor)
        mScriptEditor->openFile(path);
}

void EditorApplication::openImageEditor(const char* path)
{
    if (!mImageEditor)
        return;
    mImageEditor->open() = true;
    mImageEditor->openImage(path);
}

void EditorApplication::focusOnObject(GameObject& object)
{
    if (mSceneViewport)
        mSceneViewport->focusOn(object);
}

void EditorApplication::log(const char* message)
{
    mConsoleText += message;
    mConsoleText.push_back('\n');
}

void EditorApplication::log(const ct::String& message)
{
    log(message.c_str());
}

EditorApplication::SceneChange EditorApplication::beginChange()
{
    SceneChange change;
    change.scene = snapshotScene();
    change.selection = mSelection.objectId();
    change.hadSelection = mSelection.hasSelection();
    return change;
}

void EditorApplication::commitChange(const char* label, const SceneChange& before)
{
    SceneCommand command;
    command.label = label;
    command.before = before.scene;
    command.after = snapshotScene();
    command.selectionBefore = before.selection;
    command.hadSelectionBefore = before.hadSelection;
    command.selectionAfter = mSelection.objectId();
    command.hadSelectionAfter = mSelection.hasSelection();
    mCommands.push(ct::detail::move(command));

    ct::String message("Do: ");
    message += label;
    log(message);
}

void EditorApplication::beginTransaction(const char* label, const SceneChange& before)
{
    if (mTransactionActive)
        return;
    mTransactionActive = true;
    mTransactionLabel = label;
    mTransactionBefore = before;
}

void EditorApplication::commitTransaction()
{
    if (!mTransactionActive)
        return;
    commitChange(mTransactionLabel.c_str(), mTransactionBefore);
    mTransactionActive = false;
    mTransactionLabel.clear();
}

ct::Json EditorApplication::snapshotScene()
{
    ct::Json document = ct::Json::object();
    document.set("root", Serializer::WriteObject(mScene.root(), &mAssets));

    if (mScenePhysics.overrideGravity)
    {
        ct::Json gravity = ct::Json::array();
        gravity.push_back(ct::Json(mScenePhysics.gravity.x));
        gravity.push_back(ct::Json(mScenePhysics.gravity.y));
        ct::Json physics = ct::Json::object();
        physics.set("gravity", gravity);
        document.set("physics", physics);
    }

    if (mSceneCursor.enabled || !mSceneCursor.image.empty())
    {
        ct::Json offset = ct::Json::array();
        offset.push_back(ct::Json(mSceneCursor.offset.x));
        offset.push_back(ct::Json(mSceneCursor.offset.y));
        ct::Json cursor = ct::Json::object();
        cursor.set("enabled", ct::Json(mSceneCursor.enabled));
        cursor.set("image", ct::Json(mSceneCursor.image));
        cursor.set("offset", offset);
        document.set("cursor", cursor);
    }
    return document;
}

void EditorApplication::restoreScene(const ct::Json& snapshot, uint64_t selectedId, bool hadSelection)
{
    const ct::Json& rootJson = snapshot["root"];
    mSelection.clear();
    mScene.clear();

    mScenePhysics = ScenePhysics();
    const ct::Json& physics = snapshot["physics"];
    if (physics.is_object())
    {
        const ct::Json& gravity = physics["gravity"];
        if (gravity.is_array() && gravity.size() >= 2)
        {
            mScenePhysics.overrideGravity = true;
            mScenePhysics.gravity = Math::Vec2((float)gravity[0].as_double(0.0), (float)gravity[1].as_double(980.0));
        }
    }
    applyPhysicsSettings();

    mSceneCursor = SceneCursor();
    const ct::Json& cursor = snapshot["cursor"];
    if (cursor.is_object())
    {
        mSceneCursor.enabled = cursor["enabled"].as_bool(false);
        mSceneCursor.image = cursor["image"].as_cstr("");
        const ct::Json& offset = cursor["offset"];
        if (offset.is_array() && offset.size() >= 2)
            mSceneCursor.offset = Math::Vec2((float)offset[0].as_double(0.0), (float)offset[1].as_double(0.0));
    }
    applyCursorSettings();

    GameObject& root = mScene.root();
    root.setName(rootJson["name"].as_cstr("Scene"));
    root.setTag(rootJson["tag"].as_cstr(""));
    root.setActive(rootJson["active"].as_bool(true));
    root.setVisible(rootJson["visible"].as_bool(true));
    root.setLocked(rootJson["locked"].as_bool(false));
    root.setZIndex(static_cast<int>(rootJson["zIndex"].as_int(0)));
    root.setPosition(readVec2(rootJson["position"], Math::Vec2(0.0f)));
    root.setRotationDegrees(static_cast<float>(rootJson["rotation"].as_double(0.0)));
    root.setScale(readVec2(rootJson["scale"], Math::Vec2(1.0f)));

    const ct::Json& children = rootJson["children"];
    if (children.is_array())
        for (size_t i = 0; i < children.size(); ++i)
            Serializer::ReadObject(mScene, children[i], &root, &mAssets);

    if (hadSelection)
    {
        mSelection.selectId(selectedId);
        if (!mSelection.resolve(mScene))
            mSelection.clear();
    }
}

void EditorApplication::startPlay()
{
    // Play simulates the real scene in place -- the same GameObjects the
    // Hierarchy/Viewport already show, not a disconnected clone -- so
    // anything a script spawns or destroys is visible immediately. The
    // snapshot is what makes that safe: Stop discards every change physics
    // and scripts made by restoring it, the same mechanism Prefab Mode
    // already uses to discard.
    mPlaySnapshot = snapshotScene();
    mPlaySelectionId = mSelection.objectId();
    mPlayHadSelection = mSelection.hasSelection();
    ZenBlackboard::clear();

    RouteZenScriptCollisions(mScene);
    RouteZenScriptAnimationEvents(mScene);
    RouteZenScriptActionEvents(mScene);
    RouteZenScriptParticleHits(mScene);
    mScene.setSimulationEnabled(true);
    applyPhysicsSettings();

    SetZenScriptsEnabled(true);
    mPlaying = true;
    mPaused = false;
    log("Play: simulating the real scene");
    mToasts.info("Play");
}

Math::Vec2 EditorApplication::effectiveGravity() const
{
    return mScenePhysics.overrideGravity ? mScenePhysics.gravity : mProject.physics().gravity;
}

void EditorApplication::applyPhysicsSettings()
{
    const PhysicsSettings& physics = mProject.physics();
    mScene.setGravity(effectiveGravity());
    mScene.setFixedTimeStep(physics.fixedTimeStep);
    mScene.setVelocityIterations(physics.velocityIterations);
    mScene.setTreeBroadphase(physics.treeBroadphase);
}

void EditorApplication::applyCursorSettings()
{
    MouseCursor& cursor = GetMouseCursor();
    Texture* texture = mSceneCursor.image.empty() ? nullptr : loadOrGetTexture(mSceneCursor.image);
    cursor.setTexture(texture);
    cursor.setOffset(mSceneCursor.offset);
    cursor.setEnabled(mSceneCursor.enabled && texture != nullptr);
}

void EditorApplication::stopPlay()
{
    SetZenScriptsEnabled(false);
    mScene.setSimulationEnabled(false);
    GetAudio().StopAll();
    GetScreenFade().SetClear();
    restoreScene(mPlaySnapshot, mPlayHadSelection ? mPlaySelectionId : 0, mPlayHadSelection);
    mPlaySnapshot = ct::Json();
    ZenBlackboard::clear();
    mPlaying = false;
    mPaused = false;
    log("Stopped preview");
}

void EditorApplication::stepPlay()
{
    if (mPlaying && mPaused)
    {
        mScene.update(1.0f / 60.0f);
        DispatchZenScriptEvents(mScene.root());
    }
}

void EditorApplication::runStandalone()
{
    if (mCurrentScenePath.empty())
    {
        mToasts.error("Save the scene before running it in a game window");
        log("Run requires a saved scene");
        return;
    }
    if (!saveScene(mCurrentScenePath.c_str()))
        return;

#if defined(__unix__) || defined(__APPLE__)
    const std::filesystem::path runnerPath = findEditorTool("k2d_runner");
    if (!std::filesystem::exists(runnerPath))
    {
        ct::String message("Runner executable was not found: ");
        message += runnerPath.string().c_str();
        log(message);
        mToasts.error("Build k2d_runner before using Run");
        return;
    }

    const pid_t pid = fork();
    if (pid == 0)
    {
        if (mProject.valid())
            execl(runnerPath.c_str(), runnerPath.c_str(), mCurrentScenePath.c_str(), mProject.projectFile().c_str(),
                  static_cast<char*>(nullptr));
        else
            execl(runnerPath.c_str(), runnerPath.c_str(), mCurrentScenePath.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }
    if (pid < 0)
    {
        log("Could not launch standalone runner");
        mToasts.error("Could not start game runner");
        return;
    }
    log("Run: standalone game window launched");
    mToasts.info("Game runner started");
#else
    log("Standalone runner launch is not implemented on this platform");
    mToasts.error("Standalone runner is unavailable on this platform");
#endif
}

void EditorApplication::exportWeb(bool runAfterExport)
{
    if (!mProject.valid())
    {
        mToasts.error("Open a project before exporting for Web");
        return;
    }
    ct::String sceneRelative = relativeToRoot(mCurrentScenePath, mProject.root());
    if (sceneRelative.empty())
        sceneRelative = mProject.startupScene();
    if (sceneRelative.empty())
    {
        mToasts.error("Save a scene inside the project before exporting for Web");
        return;
    }
    if (!mCurrentScenePath.empty() && !saveScene(mCurrentScenePath.c_str()))
        return;
    if (!EditorFileSystem::exists(EditorFileSystem::join(mProject.root(), sceneRelative.c_str())))
    {
        mToasts.error("The Web scene must be inside the project folder");
        return;
    }

#if defined(__unix__) || defined(__APPLE__)
    const std::filesystem::path exporter = findEditorTool("export_web.sh", "K2D_WEB_EXPORTER");
    if (!std::filesystem::exists(exporter))
    {
        ct::String message("Web exporter was not found: ");
        message += exporter.string().c_str();
        log(message);
        mToasts.error("Web exporter script was not found");
        return;
    }
    const pid_t pid = fork();
    if (pid == 0)
    {
        const std::string parentPid = std::to_string(static_cast<long long>(getppid()));
        if (runAfterExport)
            execl(exporter.c_str(), exporter.c_str(), mProject.root().c_str(), "--scene", sceneRelative.c_str(),
                  "--run", "--parent-pid", parentPid.c_str(), static_cast<char*>(nullptr));
        else
            execl(exporter.c_str(), exporter.c_str(), mProject.root().c_str(), "--scene", sceneRelative.c_str(),
                  static_cast<char*>(nullptr));
        _exit(127);
    }
    if (pid < 0)
    {
        mToasts.error("Could not start Web export");
        return;
    }
    ct::String message(runAfterExport ? "Run Web: exporting current scene: " : "Export Web: exporting current scene: ");
    message += sceneRelative;
    log(message);
    mToasts.info(runAfterExport ? "Building current scene for Web" : "Exporting current scene for Web");
#elif defined(_WIN32)
    const std::filesystem::path exporter = findEditorTool("export_web.bat", "K2D_WEB_EXPORTER");
    if (!std::filesystem::exists(exporter))
    {
        mToasts.error("Web exporter batch file was not found");
        return;
    }
    const char* commandProcessor = std::getenv("COMSPEC");
    if (!commandProcessor || !commandProcessor[0])
        commandProcessor = "cmd.exe";
    std::string command = quoteWindowsArgument(commandProcessor);
    command += " /d /s /c \"";
    command += quoteWindowsArgument(exporter.string());
    command += " ";
    command += quoteWindowsArgument(mProject.root().c_str());
    command += " --scene ";
    command += quoteWindowsArgument(sceneRelative.c_str());
    if (runAfterExport)
    {
        command += " --run --parent-pid ";
        command += std::to_string(static_cast<unsigned long>(GetCurrentProcessId()));
    }
    command += "\"";
    std::vector<char> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back('\0');
    STARTUPINFOA startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    if (!CreateProcessA(commandProcessor, mutableCommand.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr,
                        &startup, &process))
    {
        mToasts.error("Could not start Web export");
        return;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    mToasts.info(runAfterExport ? "Building current scene for Web" : "Exporting current scene for Web");
#else
    (void)runAfterExport;
    mToasts.error("Web export launch is not implemented on this platform");
#endif
}

void EditorApplication::tickEditPreview(GameObject& object, float deltaTime)
{
    if (!object.isActiveInHierarchy())
        return;

    const size_t particleCount = object.componentCount<ParticleComponent>();
    for (size_t i = 0; i < particleCount; ++i)
    {
        ParticleComponent* particle = object.getComponentAt<ParticleComponent>(i);
        if (!particle || !particle->active())
            continue;
        if (particle->followOwner())
            particle->system().SetEmitterPosition(object.globalPosition());
        particle->system().Update(deltaTime);
    }

    const size_t animCount = object.componentCount<Animation2D>();
    for (size_t i = 0; i < animCount; ++i)
    {
        Animation2D* anim = object.getComponentAt<Animation2D>(i);
        if (anim && anim->active())
            anim->Advance(deltaTime);
    }

    const size_t actionCount = object.componentCount<ActionSequence2D>();
    for (size_t i = 0; i < actionCount; ++i)
    {
        ActionSequence2D* sequence = object.getComponentAt<ActionSequence2D>(i);
        if (sequence && sequence->active() && sequence->playing())
            sequence->Advance(deltaTime);
    }

    const size_t cameraCount = object.componentCount<CameraComponent>();
    for (size_t i = 0; i < cameraCount; ++i)
    {
        CameraComponent* camera = object.getComponentAt<CameraComponent>(i);
        if (camera && camera->active())
            camera->syncFromOwner();
    }

    for (size_t i = 0; i < object.childCount(); ++i)
        tickEditPreview(*object.child(i), deltaTime);
}

void EditorApplication::restartEditPreview()
{
    restartEditPreview(mScene.root());
}

void EditorApplication::restartEditPreview(GameObject& object)
{
    const size_t particleCount = object.componentCount<ParticleComponent>();
    for (size_t i = 0; i < particleCount; ++i)
        if (ParticleComponent* particle = object.getComponentAt<ParticleComponent>(i))
            particle->system().Reset();

    const size_t animCount = object.componentCount<Animation2D>();
    for (size_t i = 0; i < animCount; ++i)
        if (Animation2D* anim = object.getComponentAt<Animation2D>(i))
            anim->reset();

    const size_t actionCount = object.componentCount<ActionSequence2D>();
    for (size_t i = 0; i < actionCount; ++i)
        if (ActionSequence2D* sequence = object.getComponentAt<ActionSequence2D>(i))
            sequence->restart();

    for (size_t i = 0; i < object.childCount(); ++i)
        restartEditPreview(*object.child(i));
}

void EditorApplication::undo()
{
    const SceneCommand* command = mCommands.undo();
    if (!command)
    {
        log("Undo: nothing to undo");
        return;
    }
    restoreScene(command->before, command->selectionBefore, command->hadSelectionBefore);
    ct::String message("Undo: ");
    message += command->label;
    log(message);
}

void EditorApplication::redo()
{
    const SceneCommand* command = mCommands.redo();
    if (!command)
    {
        log("Redo: nothing to redo");
        return;
    }
    restoreScene(command->after, command->selectionAfter, command->hadSelectionAfter);
    ct::String message("Redo: ");
    message += command->label;
    log(message);
}

void EditorApplication::handleShortcuts()
{
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput)
        return;
    if (ImGui::IsKeyPressed(ImGuiKey_F9, false))
    {
        requestScreenshot();
    }
    else if (ImGui::IsKeyPressed(ImGuiKey_F10, false))
    {
        toggleGifCapture();
    }
    else if (mPlaying && ImGui::IsKeyPressed(ImGuiKey_F6, false))
    {
        reloadChangedScripts();
    }
    else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
    {
        if (io.KeyShift)
            redo();
        else
            undo();
    }
    else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false))
    {
        redo();
    }
    else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false) && !mCurrentScenePath.empty())
    {
        saveScene(mCurrentScenePath.c_str());
    }
}

void EditorApplication::requestScreenshot()
{
    mCaptureScreenshotPending = true;
    mToasts.info("Saving screenshot...");
}

void EditorApplication::toggleGifCapture()
{
    if (mDevice.IsGifCapturing())
    {
        mDevice.StopGifCapture();
        mToasts.success("GIF saved in the current working directory");
        return;
    }

    mDevice.StartGifCapture(30);
    mToasts.info("GIF recording started at 30 FPS — press F10 to stop");
}

void EditorApplication::reloadChangedScripts()
{
    if (!mPlaying)
    {
        mToasts.info("Start Play before reloading scripts");
        return;
    }

    const size_t reloaded = ReloadChangedZenScripts();
    char message[80];
    if (reloaded)
        std::snprintf(message, sizeof(message), "Reloaded %d saved script(s)", static_cast<int>(reloaded));
    else
        std::snprintf(message, sizeof(message), "No saved script changes to reload");
    log(message);
    mToasts.info(message);
}

void EditorApplication::newScene()
{
    mSelection.clear();
    mScene.clear();
    mScene.root().setName("Scene");
    mSelection.select(&mScene.root());
    mPlaying = false;
    mPaused = false;
    mCommands.clear();
    mTransactionActive = false;
    mCurrentScenePath.clear();
    mSettings.lastScenePath.clear();
    mScenePhysics = ScenePhysics();
    applyPhysicsSettings();
    log("New scene");
}

void EditorApplication::createShapesExampleScene()
{
    newScene();
    mScene.root().setName("Shapes Example");

    GameObject* filledCircle = mScene.createObject("Filled Circle");
    filledCircle->setPosition(Math::Vec2(-150.0f, 0.0f));
    CircleShape* circle = filledCircle->addComponent<CircleShape>();
    circle->setRadius(70.0f);
    circle->setColor(80, 170, 255, 255);

    GameObject* outlineCircle = mScene.createObject("Circle Outline");
    outlineCircle->setPosition(Math::Vec2(50.0f, 0.0f));
    CircleShape* lineCircle = outlineCircle->addComponent<CircleShape>();
    lineCircle->setRadius(70.0f);
    lineCircle->setMode(ShapeRenderMode::Line);
    lineCircle->setLineWidth(6.0f);
    lineCircle->setColor(255, 210, 90, 255);

    GameObject* filledRect = mScene.createObject("Filled Rect");
    filledRect->setPosition(Math::Vec2(-50.0f, 150.0f));
    RectShape* rect = filledRect->addComponent<RectShape>();
    rect->setSize(Math::Vec2(180.0f, 80.0f));
    rect->setColor(115, 220, 145, 255);

    GameObject* outlineRect = mScene.createObject("Rect Outline");
    outlineRect->setPosition(Math::Vec2(170.0f, 150.0f));
    RectShape* lineRect = outlineRect->addComponent<RectShape>();
    lineRect->setSize(Math::Vec2(160.0f, 80.0f));
    lineRect->setMode(ShapeRenderMode::Line);
    lineRect->setLineWidth(6.0f);
    lineRect->setColor(240, 120, 170, 255);

    mSelection.select(filledCircle);
    log("Created Shapes example scene. Press Play to preview it.");
}

void EditorApplication::createPhysicsExampleScene()
{
    newScene();
    mScene.root().setName("Physics Example");

    GameObject* ground = mScene.createObject("Ground");
    ground->setPosition(Math::Vec2(0.0f, 270.0f));
    RectShape* groundShape = ground->addComponent<RectShape>();
    groundShape->setSize(Math::Vec2(800.0f, 40.0f));
    groundShape->setColor(90, 105, 125, 255);
    ground->addComponent<RigidBody2D>()->setBodyType(k2d::BodyType::Static);
    ground->addComponent<BoxCollider2D>()->setSize(Math::Vec2(800.0f, 40.0f));

    GameObject* ball = mScene.createObject("Ball");
    ball->setPosition(Math::Vec2(-100.0f, -180.0f));
    CircleShape* ballShape = ball->addComponent<CircleShape>();
    ballShape->setRadius(32.0f);
    ballShape->setColor(85, 175, 255, 255);
    ball->addComponent<RigidBody2D>();
    ball->addComponent<CircleCollider2D>()->setRadius(32.0f);

    GameObject* box = mScene.createObject("Box");
    box->setPosition(Math::Vec2(120.0f, -80.0f));
    RectShape* boxShape = box->addComponent<RectShape>();
    boxShape->setSize(Math::Vec2(64.0f, 64.0f));
    boxShape->setColor(245, 180, 80, 255);
    box->addComponent<RigidBody2D>();
    box->addComponent<BoxCollider2D>()->setSize(Math::Vec2(64.0f, 64.0f));

    mSelection.select(ball);
    log("Created Physics example scene. Enable Physics Debug in Game and press Play.");
}

void EditorApplication::createBunnymarkExampleScene()
{
    newScene();
    mScene.root().setName("Bunnymark Example");

    GameObject* main = mScene.createObject("Bunnymark Main");
    ZenScriptComponent* script = main->addComponent<ZenScriptComponent>();
    if (!script->loadFile("scripts/bunnymark_main.py"))
    {
        log("Could not load scripts/bunnymark_main.py from the Assets search paths");
        mToasts.error("Bunnymark script was not found");
        return;
    }

    mSelection.select(main);
    log("Created Bunnymark example. Press Play, then click inside Game to spawn 100 bunnies.");
}

void EditorApplication::preloadTextures(const ct::Json& node)
{
    if (node.is_object())
    {
        const ct::Json::Object& members = node.members();
        for (size_t i = 0; i < members.size(); ++i)
        {
            const ct::String& key = members[i].key;
            const ct::Json& value = members[i].value;
            if ((key == "texture" || key == "normalMap") && value.is_string())
            {
                const char* name = value.as_cstr("");
                if (name[0] && !mAssets.GetTexture(name))
                {
                    // Old scenes stored absolute paths; resolve() leaves those
                    // alone so they still open, and saving rewrites them relative.
                    const ct::String file = EditorFileSystem::resolve(mProject.root(), name);
                    mAssets.LoadTexture(name, file.c_str(), true, false);
                }
            }
            else
            {
                preloadTextures(value);
            }
        }
    }
    else if (node.is_array())
    {
        for (size_t i = 0; i < node.size(); ++i)
            preloadTextures(node[i]);
    }
}

Texture* EditorApplication::loadOrGetTexture(const ct::String& imagePath)
{
    // The asset NAME is what the serializer writes into the scene, so it must
    // be project-relative or the scene only opens on this machine. The path
    // used to actually read the file stays absolute.
    const ct::String name = EditorFileSystem::relativeTo(mProject.root(), imagePath);
    Texture* texture = mAssets.GetTexture(name.c_str());
    if (!texture)
        texture = mAssets.LoadTexture(name.c_str(), imagePath.c_str(), true, false);
    return texture;
}

GameObject* EditorApplication::createSpriteNodeFromImage(const ct::String& imagePath, GameObject* parent,
                                                          const Math::Vec2* worldPosition)
{
    Texture* texture = loadOrGetTexture(imagePath);
    if (!texture)
    {
        log("Sprite drop failed: could not load image");
        mToasts.error("Could not load image");
        return nullptr;
    }

    GameObject* destination = parent ? parent : &mScene.root();
    const ct::String name = EditorFileSystem::withoutExtension(EditorFileSystem::fileName(imagePath));

    const SceneChange before = beginChange();
    GameObject* created = mScene.createObject(name.empty() ? "Sprite" : name.c_str(), destination);
    if (!created)
    {
        mToasts.error("Could not create sprite node");
        return nullptr;
    }
    SpriteComponent* sprite = created->addComponent<SpriteComponent>(texture);
    sprite->setSize(Math::Vec2(static_cast<float>(texture->Width()), static_cast<float>(texture->Height())));
    if (worldPosition)
        created->setPosition(*worldPosition);

    mSelection.select(created);
    commitChange("Create Sprite from Image", before);

    ct::String message("Created sprite: ");
    message += created->name();
    log(message);
    mToasts.success(message);
    return created;
}

Texture* EditorApplication::particlePlaceholderTexture()
{
    constexpr const char* kName = "__editor_particle_placeholder";
    constexpr int kSize = 64;
    if (Texture* texture = mAssets.GetTexture(kName))
        return texture;

    unsigned char pixels[kSize * kSize * 4];
    const float half = kSize * 0.5f;
    for (int y = 0; y < kSize; ++y)
    {
        for (int x = 0; x < kSize; ++x)
        {
            const float dx = (x + 0.5f - half) / half;
            const float dy = (y + 0.5f - half) / half;
            float falloff = 1.0f - sqrtf(dx * dx + dy * dy);
            if (falloff < 0.0f)
                falloff = 0.0f;
            falloff *= falloff;
            const int i = (y * kSize + x) * 4;
            pixels[i + 0] = 255;
            pixels[i + 1] = 255;
            pixels[i + 2] = 255;
            pixels[i + 3] = static_cast<unsigned char>(falloff * 255.0f);
        }
    }
    return mAssets.CreateTexture(kName, kSize, kSize, pixels, true, false);
}

bool EditorApplication::openScene(const char* path)
{
    if (!path || !path[0])
        return false;

    FileBuffer buffer;
    if (!FileSystem::Instance().LoadFile(path, buffer, true))
    {
        ct::String message("Open scene failed, could not read: ");
        message += path;
        log(message);
        mToasts.error(message);
        return false;
    }

    ct::Json::Error err;
    const ct::Json document = ct::Json::parse(buffer.Text(), &err);
    if (err)
    {
        ct::String message("Open scene failed, invalid JSON: ");
        message += path;
        log(message);
        mToasts.error(message);
        return false;
    }

    preloadTextures(document);
    restoreScene(document, 0, false);
    mCommands.clear();
    mTransactionActive = false;
    mPlaying = false;
    mPaused = false;
    mCurrentScenePath = path;
    mSettings.lastScenePath = path;
    ct::String message("Opened scene: ");
    message += path;
    log(message);
    ct::String toast("Opened ");
    toast += EditorFileSystem::fileName(path);
    mToasts.success(toast);
    return true;
}

bool EditorApplication::saveScene(const char* path)
{
    if (!path || !path[0])
        return false;

    const ct::Json document = snapshotScene();
    if (!FileSystem::Instance().SaveTextFile(path, document.dump(2)))
    {
        ct::String message("Save scene failed: ");
        message += path;
        log(message);
        mToasts.error(message);
        return false;
    }

    mCurrentScenePath = path;
    mSettings.lastScenePath = path;
    ct::String message("Saved scene: ");
    message += path;
    log(message);
    ct::String toast("Saved ");
    toast += EditorFileSystem::fileName(path);
    mToasts.success(toast);
    return true;
}

bool EditorApplication::newProject(const char* rootDirectory, const char* name)
{
    if (!mProject.create(rootDirectory, name))
    {
        log("Could not create project");
        mToasts.error("Could not create project");
        return false;
    }

    // Lets a script/prefab "path" authored relative to the project root
    // (e.g. "assets/scripts/prota.py") resolve on any machine, the same way
    // runner/src/main.cpp already does for exported builds -- without this,
    // only absolute, single-machine paths ever worked from the editor.
    FileSystem::Instance().ResetSearchPaths();
    FileSystem::Instance().AddSearchPath(mProject.root().c_str());

    mSettings.lastProjectPath = mProject.projectFile();
    mSettings.touchRecentProject(mProject.projectFile());
    mSettings.assetsDirectory = mProject.assetsDirectory();
    newScene();
    ct::String message("Project created: ");
    message += mProject.root();
    log(message);
    ct::String toast("Project created: ");
    toast += mProject.name();
    mToasts.success(toast);
    return true;
}

bool EditorApplication::openProject(const char* projectFile)
{
    if (!mProject.load(projectFile))
    {
        log("Could not open project");
        mToasts.error("Could not open project");
        return false;
    }

    FileSystem::Instance().ResetSearchPaths();
    FileSystem::Instance().AddSearchPath(mProject.root().c_str());

    mSettings.lastProjectPath = mProject.projectFile();
    mSettings.touchRecentProject(mProject.projectFile());
    mSettings.assetsDirectory = mProject.assetsDirectory();
    if (!mProject.startupScene().empty())
        openScene(EditorFileSystem::join(mProject.root(), mProject.startupScene().c_str()).c_str());
    else
        newScene();
    ct::String message("Project opened: ");
    message += mProject.root();
    log(message);
    ct::String toast("Project opened: ");
    toast += mProject.name();
    mToasts.success(toast);
    return true;
}

void EditorApplication::openFileDialog(FileDialogPurpose purpose, ImGuiFileDialog::Mode mode,
                                       const ct::String& startDirectory, const ct::String& initialName)
{
    mFileDialogPurpose = purpose;
    mFileDialog.Open(mode, std::filesystem::path(startDirectory.c_str()), initialName.c_str());
}

bool EditorApplication::enterPrefabMode(const char* path)
{
    if (mPlaying || mPrefabModeActive || !path || !path[0])
        return false;

    Prefab prefab;
    if (!prefab.Load(path))
    {
        ct::String message("Could not open prefab: ");
        message += path;
        log(message);
        mToasts.error(message);
        return false;
    }

    mPrefabModeSavedScene = snapshotScene();
    mPrefabModeSavedPath = mCurrentScenePath;
    mPrefabModeSavedSelectionId = mSelection.objectId();
    mPrefabModeHadSelection = mSelection.hasSelection();

    mSelection.clear();
    mScene.clear();
    mCommands.clear();
    mTransactionActive = false;

    // Instantiate keeps the prefab's own name, tag and (crucially) its
    // components -- restoreScene() only ever populates a scene root's name/
    // tag/transform/children, since a real scene's implicit root never has
    // components of its own. Yes, this nests the prefab's root one level
    // under mScene.root() (an extra Hierarchy row) -- see the comment on
    // mScene.root()'s renaming below for why that is the lesser problem.
    GameObject* root = prefab.Instantiate(mScene, nullptr, &mAssets);
    if (!root)
    {
        restoreScene(mPrefabModeSavedScene, mPrefabModeHadSelection ? mPrefabModeSavedSelectionId : 0,
                    mPrefabModeHadSelection);
        mCurrentScenePath = mPrefabModeSavedPath;
        ct::String message("Could not instantiate prefab: ");
        message += path;
        log(message);
        mToasts.error(message);
        return false;
    }

    // Named distinctly from the prefab's own root so the wrapper row in the
    // Hierarchy doesn't look like an accidental duplicate of it.
    mScene.root().setName("(Prefab Root)");
    mPrefabModeRootId = root->id();
    mPrefabModePath = path;
    mCurrentScenePath.clear();
    mPrefabModeActive = true;
    mSelection.select(root);

    ct::String message("Editing prefab: ");
    message += path;
    log(message);
    return true;
}

void EditorApplication::exitPrefabMode(bool save)
{
    if (!mPrefabModeActive)
        return;

    if (save)
    {
        GameObject* root = findById(mScene.root(), mPrefabModeRootId);
        if (root)
        {
            Prefab out;
            if (out.SaveToFile(mPrefabModePath.c_str(), *root, &mAssets))
            {
                ct::String toast("Saved ");
                toast += EditorFileSystem::fileName(mPrefabModePath);
                mToasts.success(toast);
            }
            else
            {
                ct::String message("Could not save prefab: ");
                message += mPrefabModePath;
                log(message);
                mToasts.error(message);
            }
        }
        else
        {
            ct::String message("Prefab root was deleted -- nothing saved: ");
            message += mPrefabModePath;
            log(message);
            mToasts.warning(message);
        }
    }

    restoreScene(mPrefabModeSavedScene, mPrefabModeHadSelection ? mPrefabModeSavedSelectionId : 0,
                mPrefabModeHadSelection);
    mCommands.clear();
    mTransactionActive = false;
    mCurrentScenePath = mPrefabModeSavedPath;
    mPrefabModeActive = false;
    mPrefabModePath.clear();
    mPrefabModeSavedScene = ct::Json();
}

void EditorApplication::requestSaveObjectAsPrefab(uint64_t objectId, const ct::String& suggestedName)
{
    mPendingPrefabExportId = objectId;
    ct::String initialName = suggestedName.empty() ? ct::String("prefab.k2dprefab") : suggestedName;
    openFileDialog(FileDialogPurpose::SaveSelectionAsPrefab, ImGuiFileDialog::Mode::SaveFile,
                   mProject.valid() ? mProject.prefabsDirectory() : EditorFileSystem::currentDirectory(),
                   initialName);
}

void EditorApplication::drawWorkspace()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Kinetix2D Editor Workspace", nullptr, flags);
    ImGui::PopStyleVar(3);

    drawMenuBar();
    drawToolbar();
    drawPrefabModeBar();
    drawFileDialog();
    drawNewProjectNameDialog();

    const ImGuiID dockspaceId = ImGui::GetID("Kinetix2D Dockspace");
    if (mDefaultLayoutPending || mLayoutResetRequested)
    {
        if (mLayoutResetRequested || ImGui::DockBuilderGetNode(dockspaceId) == nullptr)
            createDefaultDockLayout(dockspaceId);
        mDefaultLayoutPending = false;
        mLayoutResetRequested = false;
    }
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    ImGui::End();
}

void EditorApplication::drawMenuBar()
{
    if (!ImGui::BeginMenuBar())
        return;

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("New Project..."))
            openFileDialog(FileDialogPurpose::NewProjectFolder, ImGuiFileDialog::Mode::ChooseFolder,
                           EditorFileSystem::currentDirectory());
        if (ImGui::MenuItem("Open Project..."))
            openFileDialog(FileDialogPurpose::OpenProject, ImGuiFileDialog::Mode::OpenFile,
                           EditorFileSystem::currentDirectory());
        if (ImGui::BeginMenu("Recent Projects", !mSettings.recentProjectPaths.empty()))
        {
            for (size_t i = mSettings.recentProjectPaths.size(); i > 0; --i)
            {
                const ct::String& path = mSettings.recentProjectPaths[i - 1];
                if (ImGui::MenuItem(path.c_str()))
                    openProject(path.c_str());
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Save Project", nullptr, false, mProject.valid()))
            mProject.save();

        ImGui::Separator();
        if (ImGui::MenuItem("New Scene", "Ctrl+N", false, !mPrefabModeActive))
            newScene();
        if (ImGui::BeginMenu("New Example Scene", !mPrefabModeActive))
        {
            if (ImGui::MenuItem("Shapes"))
                createShapesExampleScene();
            if (ImGui::MenuItem("Physics"))
                createPhysicsExampleScene();
            if (ImGui::MenuItem("Bunnymark"))
                createBunnymarkExampleScene();
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Open Scene...", nullptr, false, !mPrefabModeActive))
            openFileDialog(FileDialogPurpose::OpenScene, ImGuiFileDialog::Mode::OpenFile,
                           mProject.valid() ? mProject.scenesDirectory() : EditorFileSystem::currentDirectory());
        if (ImGui::MenuItem("Save Scene", "Ctrl+S", false, !mPrefabModeActive && !mCurrentScenePath.empty()))
            saveScene(mCurrentScenePath.c_str());
        if (ImGui::MenuItem("Save Scene As...", nullptr, false, !mPrefabModeActive))
            openFileDialog(FileDialogPurpose::SaveScene, ImGuiFileDialog::Mode::SaveFile,
                           mProject.valid() ? mProject.scenesDirectory() : EditorFileSystem::currentDirectory(),
                           "scene.k2dscene");

        ImGui::Separator();
        if (ImGui::MenuItem("New Prefab...", nullptr, false, !mPrefabModeActive))
            openFileDialog(FileDialogPurpose::NewPrefab, ImGuiFileDialog::Mode::SaveFile,
                           mProject.valid() ? mProject.prefabsDirectory() : EditorFileSystem::currentDirectory(),
                           "prefab.k2dprefab");
        if (ImGui::MenuItem("Open Prefab...", nullptr, false, !mPrefabModeActive))
            openFileDialog(FileDialogPurpose::OpenPrefab, ImGuiFileDialog::Mode::OpenFile,
                           mProject.valid() ? mProject.prefabsDirectory() : EditorFileSystem::currentDirectory());

        ImGui::Separator();
        if (ImGui::MenuItem("Capture Screenshot", "F9"))
            requestScreenshot();
        const char* gifCaptureLabel = mDevice.IsGifCapturing() ? "Stop GIF Recording" : "Record GIF (30 FPS)";
        if (ImGui::MenuItem(gifCaptureLabel, "F10"))
            toggleGifCapture();

        if (mProject.valid())
        {
            ImGui::Separator();
            const ct::String relative = relativeToRoot(mCurrentScenePath, mProject.root());
            if (ImGui::MenuItem("Add Current Scene to Project", nullptr, false, !relative.empty()))
            {
                mProject.addScene(relative.c_str());
                if (mProject.scenes().size() == 1)
                    mProject.setStartupScene(relative.c_str());
                mProject.save();
                ct::String message("Added to project: ");
                message += relative;
                log(message);
            }
            if (ImGui::BeginMenu("Scenes", !mProject.scenes().empty()))
            {
                for (size_t i = 0; i < mProject.scenes().size(); ++i)
                {
                    const ct::String& sceneRelative = mProject.scenes()[i];
                    ct::String label = sceneRelative;
                    if (mProject.startupScene() == sceneRelative)
                        label += "  (startup)";
                    if (ImGui::MenuItem(label.c_str()))
                        openScene(EditorFileSystem::join(mProject.root(), sceneRelative.c_str()).c_str());
                }
                ImGui::EndMenu();
            }
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit"))
    {
        ct::String undoLabel("Undo");
        if (canUndo())
        {
            undoLabel += " ";
            undoLabel += undoName();
        }
        if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, canUndo()))
            undo();

        ct::String redoLabel("Redo");
        if (canRedo())
        {
            redoLabel += " ";
            redoLabel += redoName();
        }
        if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y", false, canRedo()))
            redo();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Project"))
    {
        if (ImGui::MenuItem(ICON_MDI_EXPORT " Export Web...", nullptr, false, mProject.valid()))
            exportWeb(false);
        if (ImGui::MenuItem(ICON_MDI_WEB " Run Web", nullptr, false, mProject.valid()))
            exportWeb(true);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Exports bytecode and assets, then serves the game on a free localhost port");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window"))
    {
        for (const ct::Unique<EditorPanel>& panel : mPanels)
        {
            if (ImGui::MenuItem(panel->title(), nullptr, &panel->open()))
            {
                ct::String message(panel->open() ? "Panel shown: " : "Panel hidden: ");
                message += panel->title();
                log(message);
            }
        }
        ImGui::Separator();
        ImGui::MenuItem("Profiler", nullptr, &mProfilerOpen);
        if (ImGui::MenuItem("Reload Saved Scripts", "F6", false, mPlaying))
            reloadChangedScripts();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Recompile changed .py files once; files are never polled automatically");
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Layout"))
        {
            mLayoutResetRequested = true;
            log("Layout reset");
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View"))
    {
        if (ImGui::BeginMenu("Theme"))
        {
            for (int index = 0; index < kEditorThemeCount; ++index)
            {
                const EditorThemeKind kind = static_cast<EditorThemeKind>(index);
                if (ImGui::MenuItem(editorThemeName(kind), nullptr, mThemeKind == index))
                {
                    mThemeKind = index;
                    applyEditorTheme(kind);
                    ct::String message("Theme: ");
                    message += editorThemeName(kind);
                    log(message);
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}

void EditorApplication::drawFileDialog()
{
    if (mFileDialogPurpose == FileDialogPurpose::None)
        return;

    const std::filesystem::path anchor = mProject.valid()
                                             ? std::filesystem::path(mProject.root().c_str())
                                             : std::filesystem::path(EditorFileSystem::currentDirectory().c_str());
    if (!mFileDialog.Render(anchor, anchor, anchor))
        return;

    const ImGuiFileDialog::Result result = mFileDialog.ConsumeResult();
    const FileDialogPurpose purpose = mFileDialogPurpose;
    mFileDialogPurpose = FileDialogPurpose::None;
    if (!result.accepted)
        return;

    const ct::String path(result.path.string().c_str());
    switch (purpose)
    {
    case FileDialogPurpose::NewProjectFolder:
        mNewProjectParentDirectory = path;
        mNewProjectName[0] = '\0';
        ImGui::OpenPopup("New Project");
        break;
    case FileDialogPurpose::OpenProject:
        openProject(path.c_str());
        break;
    case FileDialogPurpose::OpenScene:
        openScene(path.c_str());
        break;
    case FileDialogPurpose::SaveScene:
        saveScene(path.c_str());
        break;
    case FileDialogPurpose::NewPrefab:
    {
        Scene scratch;
        GameObject* obj = scratch.createObject(EditorFileSystem::withoutExtension(EditorFileSystem::fileName(path)).c_str());
        Prefab prefab;
        if (obj && prefab.SaveToFile(path.c_str(), *obj, &mAssets))
            enterPrefabMode(path.c_str());
        else
        {
            ct::String message("Could not create prefab: ");
            message += path;
            log(message);
            mToasts.error(message);
        }
        break;
    }
    case FileDialogPurpose::OpenPrefab:
        enterPrefabMode(path.c_str());
        break;
    case FileDialogPurpose::SaveSelectionAsPrefab:
    {
        GameObject* object = findById(mScene.root(), mPendingPrefabExportId);
        if (object)
        {
            Prefab prefab;
            if (prefab.SaveToFile(path.c_str(), *object, &mAssets))
            {
                ct::String toast("Saved ");
                toast += EditorFileSystem::fileName(path);
                mToasts.success(toast);
            }
            else
            {
                ct::String message("Could not save prefab: ");
                message += path;
                log(message);
                mToasts.error(message);
            }
        }
        break;
    }
    default:
        break;
    }
}

void EditorApplication::drawNewProjectNameDialog()
{
    if (!ImGui::BeginPopupModal("New Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::TextUnformatted("Choose a name for the project folder.");
    ImGui::TextDisabled("Location: %s", mNewProjectParentDirectory.c_str());
    ImGui::SetNextItemWidth(300.0f);
    ImGui::InputTextWithHint("Name", "My Game", mNewProjectName, sizeof(mNewProjectName));

    const bool hasName = mNewProjectName[0] != '\0';
    const bool invalidName = std::strchr(mNewProjectName, '/') || std::strchr(mNewProjectName, '\\') ||
                             std::strcmp(mNewProjectName, ".") == 0 || std::strcmp(mNewProjectName, "..") == 0;
    const ct::String projectDirectory = EditorFileSystem::join(mNewProjectParentDirectory, mNewProjectName);
    const bool alreadyExists = hasName && EditorFileSystem::exists(projectDirectory);
    if (invalidName)
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.3f, 1.0f), "The name cannot contain a path separator.");
    else if (alreadyExists)
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.3f, 1.0f), "This project folder already exists.");

    ImGui::TextDisabled("Will create: scenes, assets/scripts and assets/prefabs.");
    ImGui::BeginDisabled(!hasName || invalidName || alreadyExists);
    if (ImGui::Button("Create Project"))
    {
        if (newProject(mNewProjectParentDirectory.c_str(), mNewProjectName))
        {
            mNewProjectParentDirectory.clear();
            mNewProjectName[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        mNewProjectParentDirectory.clear();
        mNewProjectName[0] = '\0';
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void EditorApplication::drawPrefabModeBar()
{
    if (!mPrefabModeActive)
        return;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_TabActive));
    ImGui::BeginChild("##prefabModeBar", ImVec2(0.0f, 30.0f), false, ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPos(ImVec2(10.0f, 5.0f));
    ImGui::Text(ICON_MDI_CUBE_OUTLINE " Editing Prefab: %s",
               EditorFileSystem::fileName(mPrefabModePath).c_str());
    ImGui::SameLine(ImGui::GetWindowWidth() - 190.0f);
    if (ImGui::Button("Save & Close", ImVec2(90.0f, 20.0f)))
        exitPrefabMode(true);
    ImGui::SameLine();
    if (ImGui::Button("Discard", ImVec2(80.0f, 20.0f)))
        exitPrefabMode(false);
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void EditorApplication::drawStatusBar()
{
    const float deltaTime = mDevice.DeltaTime();
    mStatsSmoothedDelta = mStatsSmoothedDelta <= 0.0f ? deltaTime : mStatsSmoothedDelta * 0.9f + deltaTime * 0.1f;
    const float fps = mStatsSmoothedDelta > 0.0f ? 1.0f / mStatsSmoothedDelta : 0.0f;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float height = 26.0f;
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - height));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, height));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoDocking;
    ImGui::Begin("##StatusBar", nullptr, flags);

    ImGui::Text("%.0f FPS  %.2f ms", fps, mStatsSmoothedDelta * 1000.0f);
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::Text("Objects: %zu", mScene.objectCount());
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    if (mSelection.hasSelection())
        ImGui::Text("Selected: %zu", mSelection.count());
    else
        ImGui::TextDisabled("No selection");
    if (mProject.valid())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::Text("Project: %s", mProject.name().c_str());
    }
    if (!mCurrentScenePath.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::Text("Scene: %s", EditorFileSystem::fileName(mCurrentScenePath).c_str());
    }

    ImGui::End();
}

void EditorApplication::drawToolbar()
{
    ImGui::BeginChild("##toolbar", ImVec2(0.0f, 34.0f), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::SetCursorPos(ImVec2(8.0f, 4.0f));

    if (toolbarIcon("new", ICON_MDI_FILE_DOCUMENT_OUTLINE, "New scene", false, !mPrefabModeActive))
        newScene();
    toolbarSameLine();
    if (toolbarIcon("open", ICON_MDI_FOLDER_OPEN, "Open scene", false, !mPrefabModeActive))
        openFileDialog(FileDialogPurpose::OpenScene, ImGuiFileDialog::Mode::OpenFile,
                       mProject.valid() ? mProject.scenesDirectory() : EditorFileSystem::currentDirectory());
    toolbarSameLine();
    if (toolbarIcon("save", ICON_MDI_CONTENT_SAVE, "Save scene", false, !mPrefabModeActive))
    {
        if (!mCurrentScenePath.empty())
            saveScene(mCurrentScenePath.c_str());
        else
            openFileDialog(FileDialogPurpose::SaveScene, ImGuiFileDialog::Mode::SaveFile,
                           mProject.valid() ? mProject.scenesDirectory() : EditorFileSystem::currentDirectory(),
                           "scene.k2dscene");
    }
    toolbarDivider();
    if (toolbarIcon("undo", ICON_MDI_UNDO, canUndo() ? undoName() : "Nothing to undo", false, canUndo()))
        undo();
    toolbarSameLine();
    if (toolbarIcon("redo", ICON_MDI_REDO, canRedo() ? redoName() : "Nothing to redo", false, canRedo()))
        redo();

    const float playbackWidth = 30.0f * 5.0f + 3.0f * 4.0f;
    ImGui::SameLine();
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - playbackWidth) * 0.5f);
    if (toolbarIcon("play", ICON_MDI_PLAY, "Play", mPlaying && !mPaused, !mPlaying && !mPrefabModeActive))
        startPlay();
    toolbarSameLine();
    if (toolbarIcon("run", ICON_MDI_LAUNCH, "Run in a standalone game window", false, !mPrefabModeActive))
        runStandalone();
    toolbarSameLine();
    if (toolbarIcon("pause", ICON_MDI_PAUSE, "Pause", mPaused, mPlaying))
    {
        mPaused = !mPaused;
        log(mPaused ? "Runtime paused" : "Runtime resumed");
    }
    toolbarSameLine();
    if (toolbarIcon("step", ICON_MDI_STEP_FORWARD, "Advance one frame", false, mPlaying && mPaused))
        stepPlay();
    toolbarSameLine();
    if (toolbarIcon("stop", ICON_MDI_STOP, "Stop", false, mPlaying))
        stopPlay();
    ImGui::EndChild();
}

void EditorApplication::createDefaultDockLayout(unsigned int dockspaceId)
{
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

    ImGuiID center = dockspaceId;
    ImGuiID left = 0;
    ImGuiID right = 0;
    ImGuiID bottom = 0;
    ImGuiID game = 0;
    ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.20f, &left, &center);
    ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.25f, &right, &center);
    ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.25f, &bottom, &center);
    ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.35f, &game, &center);

    ImGui::DockBuilderDockWindow("Hierarchy", left);
    ImGui::DockBuilderDockWindow("Inspector", right);
    ImGui::DockBuilderDockWindow("Prefabs", right);
    ImGui::DockBuilderDockWindow("Scene", center);
    ImGui::DockBuilderDockWindow("Game", game);
    ImGui::DockBuilderDockWindow("Particles", game);
    ImGui::DockBuilderDockWindow("Tile Painter", game);
    ImGui::DockBuilderDockWindow("Sprite Editor", game);
    ImGui::DockBuilderDockWindow("Console", bottom);
    ImGui::DockBuilderDockWindow("Assets", bottom);
    ImGui::DockBuilderDockWindow("Atlas", bottom);
    ImGui::DockBuilderDockWindow("Profiler", bottom);
    ImGui::DockBuilderDockWindow("Scripts", bottom);
    ImGui::DockBuilderDockWindow("Script Editor", center);
    ImGui::DockBuilderFinish(dockspaceId);
    for (const ct::Unique<EditorPanel>& panel : mPanels)
        panel->open() = panel->openByDefault();
    mProfilerOpen = false;
    mDefaultFocusPending = true;
}

} // namespace k2d::editor
