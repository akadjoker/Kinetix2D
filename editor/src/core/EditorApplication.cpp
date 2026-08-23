#include "EditorApplication.h"

#include "EditorFileSystem.h"
#include "EditorPanel.h"
#include "EditorTheme.h"
#include "panels/AssetsPanel.h"
#include "panels/ConsolePanel.h"
#include "panels/GamePanel.h"
#include "panels/HierarchyPanel.h"
#include "panels/InspectorPanel.h"
#include "panels/ParticlePanel.h"
#include "panels/PrefabsPanel.h"
#include "panels/ScriptsPanel.h"
#include "panels/SceneViewportPanel.h"
#include "widgets/EditorToolbar.h"

#include <glad/glad.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <IconsMaterialDesignIcons.h>
#include <k2d/Animation2D.h>
#include <k2d/FileBuffer.h>
#include <k2d/FileSystem.h>
#include <k2d/ParticleComponent.h>
#include <k2d/Serializer.h>
#include <k2d/ZenScriptComponent.h>

#include <cmath>
#include <filesystem>

namespace k2d::editor
{

namespace
{
Math::Vec2 readVec2(const ct::Json &value, const Math::Vec2 &fallback)
{
    if (!value.is_array() || value.size() < 2)
        return fallback;
    return Math::Vec2(static_cast<float>(value[0].as_double(fallback.x)),
                      static_cast<float>(value[1].as_double(fallback.y)));
}

constexpr const char *kSettingsPath = "k2d_editor_settings.json";

ct::String relativeToRoot(const ct::String &absolute, const ct::String &root)
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
}

EditorApplication::~EditorApplication() = default;

bool EditorApplication::initialize()
{
    if (!mDevice.Init("Kinetix2D Editor", 1600, 900, true))
        return false;

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = "k2d_editor.ini";
    io.Fonts->AddFontDefault();
    if (!loadEditorIconFont(io))
        log("Material Design icon font failed to load");
    applyEditorTheme();

    RegisterZenScriptSerializer();
    SetZenScriptInput(&mDevice.GetInput());
    SetZenScriptAssets(&mAssets);
    SetZenScriptOutput([](const char *text, bool isError, void *user)
    {
        EditorApplication &self = *static_cast<EditorApplication *>(user);
        static ct::String lineBuffer;
        for (const char *c = text; *c; ++c)
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
    }, this);

    loadSettings();
    createPanels();
    particlePlaceholderTexture();
    bool opened = false;
    if (!mSettings.lastProjectPath.empty() && mProject.load(mSettings.lastProjectPath.c_str()))
    {
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
        running = mDevice.PollEvents();

        if (mPlaying && !mPaused)
        {
            mRuntimeScene.update(mDevice.DeltaTime());
            DispatchZenScriptEvents(mRuntimeScene.root());
        }
        if (!mPlaying && mSettings.viewportLivePreview)
            tickEditPreview(mScene.root(), mDevice.DeltaTime());

        glClearColor(0.055f, 0.062f, 0.075f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        mDevice.BeginUI();
        handleShortcuts();
        drawWorkspace();
        for (const ct::Unique<EditorPanel> &panel : mPanels)
            panel->draw();
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
        mDevice.Swap();
    }
    return 0;
}

void EditorApplication::shutdown()
{
    if (!mInitialized)
        return;
    saveSettings();
    mPanels.clear();
    mSelection.clear();
    mDevice.Shutdown();
    mInitialized = false;
}

void EditorApplication::loadSettings()
{
    if (!mSettings.load(kSettingsPath))
        log("No saved editor settings found - using defaults");
}

void EditorApplication::saveSettings()
{
    if (!mSettings.save(kSettingsPath))
        log("Could not write editor settings");
}

void EditorApplication::createPanels()
{
    mPanels.push_back(ct::make_unique<HierarchyPanel>(*this));
    mPanels.push_back(ct::make_unique<InspectorPanel>(*this));
    mPanels.push_back(ct::make_unique<SceneViewportPanel>(*this));
    mPanels.push_back(ct::make_unique<GamePanel>(*this));
    mPanels.push_back(ct::make_unique<ParticlePanel>(*this));
    mPanels.push_back(ct::make_unique<AssetsPanel>(*this));
    mPanels.push_back(ct::make_unique<PrefabsPanel>(*this));
    mPanels.push_back(ct::make_unique<ScriptsPanel>(*this));
    mPanels.push_back(ct::make_unique<ConsolePanel>(*this));
}

void EditorApplication::log(const char *message)
{
    mConsoleText += message;
    mConsoleText.push_back('\n');
}

void EditorApplication::log(const ct::String &message)
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

void EditorApplication::commitChange(const char *label, const SceneChange &before)
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

void EditorApplication::beginTransaction(const char *label, const SceneChange &before)
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
    return document;
}

void EditorApplication::restoreScene(const ct::Json &snapshot, uint64_t selectedId, bool hadSelection)
{
    const ct::Json &rootJson = snapshot["root"];
    mSelection.clear();
    mScene.clear();

    GameObject &root = mScene.root();
    root.setName(rootJson["name"].as_cstr("Scene"));
    root.setTag(rootJson["tag"].as_cstr(""));
    root.setActive(rootJson["active"].as_bool(true));
    root.setVisible(rootJson["visible"].as_bool(true));
    root.setLocked(rootJson["locked"].as_bool(false));
    root.setZIndex(static_cast<int>(rootJson["zIndex"].as_int(0)));
    root.setPosition(readVec2(rootJson["position"], Math::Vec2(0.0f)));
    root.setRotationDegrees(static_cast<float>(rootJson["rotation"].as_double(0.0)));
    root.setScale(readVec2(rootJson["scale"], Math::Vec2(1.0f)));

    const ct::Json &children = rootJson["children"];
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
    mRuntimeScene.clear();
    ZenBlackboard::clear();

    const ct::Json rootJson = Serializer::WriteObject(mScene.root(), &mAssets);
    GameObject &runtimeRoot = mRuntimeScene.root();
    runtimeRoot.setName(rootJson["name"].as_cstr("Scene"));
    runtimeRoot.setTag(rootJson["tag"].as_cstr(""));
    runtimeRoot.setActive(rootJson["active"].as_bool(true));
    runtimeRoot.setVisible(rootJson["visible"].as_bool(true));
    runtimeRoot.setZIndex(static_cast<int>(rootJson["zIndex"].as_int(0)));
    runtimeRoot.setPosition(readVec2(rootJson["position"], Math::Vec2(0.0f)));
    runtimeRoot.setRotationDegrees(static_cast<float>(rootJson["rotation"].as_double(0.0)));
    runtimeRoot.setScale(readVec2(rootJson["scale"], Math::Vec2(1.0f)));

    const ct::Json &children = rootJson["children"];
    if (children.is_array())
        for (size_t i = 0; i < children.size(); ++i)
            Serializer::ReadObject(mRuntimeScene, children[i], &runtimeRoot, &mAssets);

    SetZenScriptsEnabled(true);
    mPlaying = true;
    mPaused = false;
    log("Play: runtime scene cloned from the edited scene");
    mToasts.info("Play");
}

void EditorApplication::stopPlay()
{
    SetZenScriptsEnabled(false);
    mRuntimeScene.clear();
    ZenBlackboard::clear();
    mPlaying = false;
    mPaused = false;
    log("Stopped preview");
}

void EditorApplication::stepPlay()
{
    if (mPlaying && mPaused)
    {
        mRuntimeScene.update(1.0f / 60.0f);
        DispatchZenScriptEvents(mRuntimeScene.root());
    }
}

void EditorApplication::tickEditPreview(GameObject &object, float deltaTime)
{
    if (!object.isActiveInHierarchy())
        return;

    const size_t particleCount = object.componentCount<ParticleComponent>();
    for (size_t i = 0; i < particleCount; ++i)
    {
        ParticleComponent *particle = object.getComponentAt<ParticleComponent>(i);
        if (!particle || !particle->active())
            continue;
        if (particle->followOwner())
            particle->system().SetEmitterPosition(object.globalPosition());
        particle->system().Update(deltaTime);
    }

    const size_t animCount = object.componentCount<Animation2D>();
    for (size_t i = 0; i < animCount; ++i)
    {
        Animation2D *anim = object.getComponentAt<Animation2D>(i);
        if (anim && anim->active())
            anim->Advance(deltaTime);
    }

    for (size_t i = 0; i < object.childCount(); ++i)
        tickEditPreview(*object.child(i), deltaTime);
}

void EditorApplication::restartEditPreview()
{
    restartEditPreview(mScene.root());
}

void EditorApplication::restartEditPreview(GameObject &object)
{
    const size_t particleCount = object.componentCount<ParticleComponent>();
    for (size_t i = 0; i < particleCount; ++i)
        if (ParticleComponent *particle = object.getComponentAt<ParticleComponent>(i))
            particle->system().Reset();

    const size_t animCount = object.componentCount<Animation2D>();
    for (size_t i = 0; i < animCount; ++i)
        if (Animation2D *anim = object.getComponentAt<Animation2D>(i))
            anim->reset();

    for (size_t i = 0; i < object.childCount(); ++i)
        restartEditPreview(*object.child(i));
}

void EditorApplication::undo()
{
    const SceneCommand *command = mCommands.undo();
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
    const SceneCommand *command = mCommands.redo();
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
    const ImGuiIO &io = ImGui::GetIO();
    if (io.WantTextInput)
        return;
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
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
    log("New scene");
}

void EditorApplication::preloadTextures(const ct::Json &node)
{
    if (node.is_object())
    {
        const ct::Json::Object &members = node.members();
        for (size_t i = 0; i < members.size(); ++i)
        {
            const ct::String &key = members[i].key;
            const ct::Json &value = members[i].value;
            if ((key == "texture" || key == "normalMap") && value.is_string())
            {
                const char *path = value.as_cstr("");
                if (path[0] && !mAssets.GetTexture(path))
                    mAssets.LoadTexture(path, path, true, false);
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

Texture *EditorApplication::particlePlaceholderTexture()
{
    constexpr const char *kName = "__editor_particle_placeholder";
    constexpr int kSize = 64;
    if (Texture *texture = mAssets.GetTexture(kName))
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

bool EditorApplication::openScene(const char *path)
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

bool EditorApplication::saveScene(const char *path)
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

bool EditorApplication::newProject(const char *rootDirectory, const char *name)
{
    if (!mProject.create(rootDirectory, name))
    {
        log("Could not create project");
        mToasts.error("Could not create project");
        return false;
    }

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

bool EditorApplication::openProject(const char *projectFile)
{
    if (!mProject.load(projectFile))
    {
        log("Could not open project");
        mToasts.error("Could not open project");
        return false;
    }

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
                                       const ct::String &startDirectory, const ct::String &initialName)
{
    mFileDialogPurpose = purpose;
    mFileDialog.Open(mode, std::filesystem::path(startDirectory.c_str()), initialName.c_str());
}

void EditorApplication::drawWorkspace()
{
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar |
                                   ImGuiWindowFlags_NoDocking |
                                   ImGuiWindowFlags_NoTitleBar |
                                   ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus |
                                   ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Kinetix2D Editor Workspace", nullptr, flags);
    ImGui::PopStyleVar(3);

    drawMenuBar();
    drawToolbar();
    drawFileDialog();

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
                const ct::String &path = mSettings.recentProjectPaths[i - 1];
                if (ImGui::MenuItem(path.c_str()))
                    openProject(path.c_str());
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Save Project", nullptr, false, mProject.valid()))
            mProject.save();

        ImGui::Separator();
        if (ImGui::MenuItem("New Scene", "Ctrl+N"))
            newScene();
        if (ImGui::MenuItem("Open Scene..."))
            openFileDialog(FileDialogPurpose::OpenScene, ImGuiFileDialog::Mode::OpenFile,
                          mProject.valid() ? mProject.scenesDirectory()
                                           : EditorFileSystem::currentDirectory());
        if (ImGui::MenuItem("Save Scene", "Ctrl+S", false, !mCurrentScenePath.empty()))
            saveScene(mCurrentScenePath.c_str());
        if (ImGui::MenuItem("Save Scene As..."))
            openFileDialog(FileDialogPurpose::SaveScene, ImGuiFileDialog::Mode::SaveFile,
                          mProject.valid() ? mProject.scenesDirectory()
                                           : EditorFileSystem::currentDirectory(),
                          "scene.k2dscene");

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
                    const ct::String &sceneRelative = mProject.scenes()[i];
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

    if (ImGui::BeginMenu("Window"))
    {
        for (const ct::Unique<EditorPanel> &panel : mPanels)
        {
            if (ImGui::MenuItem(panel->title(), nullptr, &panel->open()))
            {
                ct::String message(panel->open() ? "Panel shown: " : "Panel hidden: ");
                message += panel->title();
                log(message);
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Layout"))
        {
            for (const ct::Unique<EditorPanel> &panel : mPanels)
                panel->open() = true;
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
        newProject(path.c_str(), EditorFileSystem::fileName(path).c_str());
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
    default:
        break;
    }
}

void EditorApplication::drawStatusBar()
{
    const float deltaTime = mDevice.DeltaTime();
    mStatsSmoothedDelta = mStatsSmoothedDelta <= 0.0f ? deltaTime : mStatsSmoothedDelta * 0.9f + deltaTime * 0.1f;
    const float fps = mStatsSmoothedDelta > 0.0f ? 1.0f / mStatsSmoothedDelta : 0.0f;

    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    const float height = 26.0f;
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - height));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, height));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                                   ImGuiWindowFlags_NoDocking;
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

    if (toolbarIcon("new", ICON_MDI_FILE_DOCUMENT_OUTLINE, "New scene"))
        newScene();
    toolbarSameLine();
    if (toolbarIcon("open", ICON_MDI_FOLDER_OPEN, "Open scene"))
        openFileDialog(FileDialogPurpose::OpenScene, ImGuiFileDialog::Mode::OpenFile,
                      mProject.valid() ? mProject.scenesDirectory() : EditorFileSystem::currentDirectory());
    toolbarSameLine();
    if (toolbarIcon("save", ICON_MDI_CONTENT_SAVE, "Save scene"))
    {
        if (!mCurrentScenePath.empty())
            saveScene(mCurrentScenePath.c_str());
        else
            openFileDialog(FileDialogPurpose::SaveScene, ImGuiFileDialog::Mode::SaveFile,
                          mProject.valid() ? mProject.scenesDirectory()
                                           : EditorFileSystem::currentDirectory(),
                          "scene.k2dscene");
    }
    toolbarDivider();
    if (toolbarIcon("undo", ICON_MDI_UNDO, canUndo() ? undoName() : "Nothing to undo",
                    false, canUndo()))
        undo();
    toolbarSameLine();
    if (toolbarIcon("redo", ICON_MDI_REDO, canRedo() ? redoName() : "Nothing to redo",
                    false, canRedo()))
        redo();

    const float playbackWidth = 30.0f * 4.0f + 3.0f * 3.0f;
    ImGui::SameLine();
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - playbackWidth) * 0.5f);
    if (toolbarIcon("play", ICON_MDI_PLAY, "Play", mPlaying && !mPaused, !mPlaying))
        startPlay();
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

    ImGui::DockBuilderDockWindow("Assets", left);
    ImGui::DockBuilderDockWindow("Hierarchy", left);
    ImGui::DockBuilderDockWindow("Inspector", right);
    ImGui::DockBuilderDockWindow("Prefabs", right);
    ImGui::DockBuilderDockWindow("Scene", center);
    ImGui::DockBuilderDockWindow("Game", game);
    ImGui::DockBuilderDockWindow("Particles", game);
    ImGui::DockBuilderDockWindow("Console", bottom);
    ImGui::DockBuilderDockWindow("Scripts", bottom);
    ImGui::DockBuilderFinish(dockspaceId);
    mDefaultFocusPending = true;
}

}
