#include "EditorWindow.h"

#include "ProjectExport.h"
#include "core/EditorFileSystem.h"

#include <k2d/FileBuffer.h>
#include <k2d/FileSystem.h>
#include <k2d/GameObject.h>
#include <k2d/RectShape.h>
#include <k2d/Serializer.h>
#include <k2d/ZenScriptComponent.h>

#include <platform.h>

namespace rt = ig::retained;

namespace k2d::editor
{

namespace
{

const char* const kProjectFile = "project.k2dproj";
const char* const kStartupScene = "scenes/main.k2dscene";
const char* const kPlayerScript = "assets/scripts/player.py";
const char* const kWebPort = "8090";

const char* const kPlayerSource = "class Player(ScriptComponent):\n"
                                  "    speed = 240\n"
                                  "\n"
                                  "    def on_update(self, dt):\n"
                                  "        dx = 0\n"
                                  "        dy = 0\n"
                                  "        if key_down(KEY_A) or key_down(KEY_LEFT):\n"
                                  "            dx = dx - 1\n"
                                  "        if key_down(KEY_D) or key_down(KEY_RIGHT):\n"
                                  "            dx = dx + 1\n"
                                  "        if key_down(KEY_W) or key_down(KEY_UP):\n"
                                  "            dy = dy - 1\n"
                                  "        if key_down(KEY_S) or key_down(KEY_DOWN):\n"
                                  "            dy = dy + 1\n"
                                  "        self.node.translate(dx * self.speed * dt, dy * self.speed * dt)\n";

ct::String absolutePath(const ct::String& path)
{
    return EditorFileSystem::isAbsolute(path) ? path
                                              : EditorFileSystem::join(EditorFileSystem::currentDirectory(), path.c_str());
}

ct::String preferencesFile()
{
    char directory[4096];
    if (!fs_get_pref_path(directory, sizeof directory, "Kinetix2D", "Editor"))
        return ct::String();
    return EditorFileSystem::join(ct::String(directory), "editor.json");
}

ct::String besideEditor(const char* name)
{
    char base[4096];
    return fs_get_base_path(base, sizeof base) ? EditorFileSystem::join(ct::String(base), name) : ct::String(name);
}

ct::Json readJson(const ct::String& path)
{
    FileBuffer file;
    if (!EditorFileSystem::exists(path) || !FileSystem::Instance().LoadFile(path.c_str(), file, true))
        return ct::Json();
    return ct::Json::parse(file.Text());
}

} // namespace

ct::String EditorWindow::findProjectRoot(const ct::String& sceneDirectory) const
{
    ct::String directory = absolutePath(sceneDirectory);
    for (int depth = 0; depth < 4 && !directory.empty(); ++depth)
    {
        if (EditorFileSystem::exists(EditorFileSystem::join(directory, kProjectFile)))
            return directory;
        const ct::String parent = EditorFileSystem::parentPath(directory);
        if (parent == directory)
            break;
        directory = parent;
    }
    return EditorFileSystem::parentPath(absolutePath(sceneDirectory));
}

void EditorWindow::setProjectRoot(const ct::String& root)
{
    mProjectRoot = absolutePath(root);
    FileSystem& files = FileSystem::Instance();
    files.ResetSearchPaths();
    files.AddSearchPath(EditorFileSystem::join(mProjectRoot, "assets").c_str(), true);
    files.AddSearchPath(mProjectRoot.c_str(), true);
    mAssetsPanel->setRoot(mProjectRoot);
    mSpriteEditor->setRoot(mProjectRoot);
    mScriptEditor->setRoot(mProjectRoot);
    mImageEditor->setRoot(mProjectRoot);
    mAtlas->setRoot(mProjectRoot);
    rememberProject();
}

void EditorWindow::rememberProject()
{
    const ct::String file = preferencesFile();
    if (file.empty() || !EditorFileSystem::exists(EditorFileSystem::join(mProjectRoot, kProjectFile)))
        return;
    ct::Json preferences = readJson(file);
    if (!preferences.is_object())
        preferences = ct::Json::object();
    preferences.set("lastProject", ct::Json(mProjectRoot.c_str()));
    FileSystem::Instance().SaveTextFile(file.c_str(), preferences.dump(2));
}

bool EditorWindow::openLastProject()
{
    const ct::Json preferences = readJson(preferencesFile());
    const ct::String root = preferences["lastProject"].as_cstr("");
    const ct::String file = EditorFileSystem::join(root, kProjectFile);
    return !root.empty() && EditorFileSystem::exists(file) && openProject(file);
}

// The project's startup scene, else its first scene, else a new one.
bool EditorWindow::openProject(const ct::String& projectFile)
{
    const ct::Json project = readJson(projectFile);
    if (!project.is_object())
    {
        log_error("Not a project: %s", projectFile.c_str());
        return false;
    }
    const ct::String root = EditorFileSystem::parentPath(absolutePath(projectFile));
    ct::String scene;
    const char* startup = project["startupScene"].as_cstr("");
    if (startup[0] && EditorFileSystem::exists(EditorFileSystem::join(root, startup)))
        scene = EditorFileSystem::join(root, startup);
    else
    {
        ct::Vector<EditorFileEntry> entries;
        EditorFileSystem::listDirectory(EditorFileSystem::join(root, "scenes"), entries);
        for (const EditorFileEntry& entry : entries)
            if (!entry.directory && EditorFileSystem::extension(entry.path) == "k2dscene")
            {
                scene = entry.path;
                break;
            }
    }
    if (!scene.empty())
        return openScene(scene.c_str());
    setProjectRoot(root);
    newScene();
    loadPhysics();
    log_info("Opened project %s", root.c_str());
    return true;
}

// A folder with scenes and assets, and a scene whose player moves with the
// arrows or WASD, so Play does something before anything is touched.
bool EditorWindow::createProject(const ct::String& folder)
{
    const ct::String root = absolutePath(folder);
    if (EditorFileSystem::exists(EditorFileSystem::join(root, kProjectFile)))
        return openProject(EditorFileSystem::join(root, kProjectFile));
    ct::Vector<EditorFileEntry> entries;
    if (EditorFileSystem::isDirectory(root) && EditorFileSystem::listDirectory(root, entries) && !entries.empty())
    {
        log_error("%s is not empty: a new project needs a folder of its own", root.c_str());
        return false;
    }
    const char* const folders[] = {"scenes", "assets/scripts", "assets/prefabs", "assets/textures"};
    for (const char* name : folders)
        if (!EditorFileSystem::makeDirectory(EditorFileSystem::join(root, name)))
        {
            log_error("Could not make %s in %s", name, root.c_str());
            return false;
        }
    FileSystem& files = FileSystem::Instance();
    ct::Json project = ct::Json::object();
    project.set("name", ct::Json(EditorFileSystem::fileName(root).c_str()));
    project.set("startupScene", ct::Json(kStartupScene));
    ct::Json scenes = ct::Json::array();
    scenes.push_back(ct::Json(kStartupScene));
    project.set("scenes", scenes);
    if (!files.SaveTextFile(EditorFileSystem::join(root, kProjectFile).c_str(), project.dump(2)) ||
        !files.SaveTextFile(EditorFileSystem::join(root, kPlayerScript).c_str(), ct::String(kPlayerSource)))
    {
        log_error("Could not write the project files in %s", root.c_str());
        return false;
    }

    setProjectRoot(root);
    newScene();
    loadPhysics();
    GameObject* player = mScene.createObject("Player");
    if (Component* shape = Serializer::Create(*player, "RectShape"))
        static_cast<RectShape*>(shape)->setColor(90, 200, 150);
    if (!player->addComponent<ZenScriptComponent>()->loadFile(kPlayerScript))
        log_error("Could not load %s", kPlayerScript);
    refreshTree();
    if (!saveScene(EditorFileSystem::join(root, kStartupScene).c_str()))
        return false;
    sceneLoaded();
    log_info("New project %s: press Play and move the player with the arrows", root.c_str());
    return true;
}

void EditorWindow::showNewProjectDialog()
{
    auto* dialog = mApp->addFloat<rt::FileDialog>("New project: where it goes and its name");
    dialog->setMode(rt::FileDialog::Mode::Save);
    dialog->setFilter("*");
    dialog->setPath(mProjectRoot.empty() ? EditorFileSystem::currentDirectory()
                                         : EditorFileSystem::parentPath(mProjectRoot));
    dialog->setFileName("my_game");
    dialog->setRect({160.0f, 80.0f, 900.0f, 540.0f});
    dialog->accepted.connect([this](const rt::String& path)
    {
        const ct::String folder = path;
        askToSave([this, folder]() { createProject(folder); });
    });
}

void EditorWindow::showOpenProjectDialog()
{
    auto* dialog = mApp->addFloat<rt::FileDialog>("Open project");
    dialog->setMode(rt::FileDialog::Mode::Open);
    dialog->setFilter("*.k2dproj");
    dialog->setPath(mProjectRoot.empty() ? EditorFileSystem::currentDirectory() : mProjectRoot);
    dialog->setRect({160.0f, 80.0f, 900.0f, 540.0f});
    dialog->accepted.connect([this](const rt::String& path)
    {
        const ct::String file = path;
        askToSave([this, file]() { openProject(file); });
    });
}

bool EditorWindow::setStartupScene()
{
    const ct::String projectFile = EditorFileSystem::join(mProjectRoot, kProjectFile);
    if (mScenePath.empty() || mProjectRoot.empty() || !EditorFileSystem::exists(projectFile))
    {
        log_warn("Save the scene in a project first");
        return false;
    }
    const ct::String scene = EditorFileSystem::relativeTo(mProjectRoot, mScenePath);
    if (!mProjectDocument.is_object())
        mProjectDocument = ct::Json::object();
    mProjectDocument.set("startupScene", ct::Json(scene.c_str()));
    ct::Json scenes = mProjectDocument["scenes"].is_array() ? mProjectDocument["scenes"] : ct::Json::array();
    bool listed = false;
    for (std::size_t i = 0; i < scenes.size(); ++i)
        listed = listed || scene == scenes[i].as_cstr("");
    if (!listed)
        scenes.push_back(ct::Json(scene.c_str()));
    mProjectDocument.set("scenes", scenes);
    if (!FileSystem::Instance().SaveTextFile(projectFile.c_str(), mProjectDocument.dump(2)))
    {
        log_error("Could not save %s", projectFile.c_str());
        return false;
    }
    log_info("%s starts the game", scene.c_str());
    return true;
}

void EditorWindow::exportWeb(bool run)
{
    if (mPlaying)
        return;
    askToSave([this, run]()
    {
        ct::String message;
        if (buildWeb(run, message))
            log_info("%s", message.c_str());
        else
            log_error("Web export failed: %s", message.c_str());
    });
}

// The saved project as a web game in <project>/export/web; run serves it on
// 127.0.0.1 and opens it in the browser.
bool EditorWindow::buildWeb(bool run, ct::String& message)
{
    if (mProjectRoot.empty() || !EditorFileSystem::exists(EditorFileSystem::join(mProjectRoot, kProjectFile)))
    {
        message = "Open or make a project first";
        return false;
    }
    const ct::String out = EditorFileSystem::join(mProjectRoot, "export/web");
    if (!ExportWeb(mProjectRoot.c_str(), besideEditor("web").c_str(), out.c_str(), message))
        return false;
    message = "Exported to " + out;
    if (!run)
        return true;
    if (mWebServerId > 0 && process_is_running(mWebServerId))
        process_kill(mWebServerId);
#if defined(_WIN32)
    const ct::String server = besideEditor("k2d_webserver.exe");
#else
    const ct::String server = besideEditor("k2d_webserver");
#endif
    const char* args[2] = {out.c_str(), kWebPort};
    mWebServerId = process_spawn(server.c_str(), args, 2, false);
    if (mWebServerId <= 0)
    {
        message = "Could not start " + server;
        return false;
    }
    const ct::String url = ct::String("http://127.0.0.1:") + kWebPort + "/";
    if (!open_url(url.c_str()))
        log_warn("Open %s in a browser", url.c_str());
    message = "Playing on " + url;
    return true;
}

} // namespace k2d::editor

namespace k2d::editor
{

void EditorWindow::exportDesktop(bool run)
{
    if (mPlaying)
        return;
    askToSave([this, run]()
    {
        ct::String message;
        if (buildDesktop(run, message))
            log_info("%s", message.c_str());
        else
            log_error("Desktop export failed: %s", message.c_str());
    });
}

// The saved project as a game for this system in <project>/export/desktop:
// the runner named after it and data.kpak beside it; run starts it.
bool EditorWindow::buildDesktop(bool run, ct::String& message)
{
    if (mProjectRoot.empty() || !EditorFileSystem::exists(EditorFileSystem::join(mProjectRoot, kProjectFile)))
    {
        message = "Open or make a project first";
        return false;
    }
#if defined(_WIN32)
    const ct::String runner = besideEditor("k2d_runner.exe");
#else
    const ct::String runner = besideEditor("k2d_runner");
#endif
    const ct::String out = EditorFileSystem::join(mProjectRoot, "export/desktop");
    ct::String game;
    if (!ExportDesktop(mProjectRoot.c_str(), runner.c_str(), out.c_str(), game, message))
        return false;
    message = "Exported to " + game;
    if (!run)
        return true;
    if (mRunnerId > 0 && process_is_running(mRunnerId))
        process_kill(mRunnerId);
    mRunnerId = process_spawn(game.c_str(), nullptr, 0, false);
    if (mRunnerId <= 0)
    {
        message = "Could not start " + game;
        return false;
    }
    message = "Running " + game;
    return true;
}

} // namespace k2d::editor
